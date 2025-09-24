#include "compressor.h"
#include "config.h"  // For READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE
#include <stdlib.h>  // For malloc, free
#include <string.h>  // For memcpy, memcmp

/**
 * Improved compression function with benchmarking
 */
compression_metrics_t compress_with_benchmark(const register_reading_t* buffer, 
                                             size_t count, 
                                             uint8_t* output) {
    compression_metrics_t metrics = {0};
    metrics.compression_method = "Delta+RLE";
    metrics.num_samples = count;
    metrics.original_payload_size = count * READ_REGISTER_COUNT * sizeof(uint16_t);
    
    if (count == 0 || output == NULL) {
        return metrics;
    }

    // Declare all variables before any goto labels to avoid C++ compilation issues
    uint8_t temp[MAX_COMPRESSION_SIZE];
    size_t temp_index = 0;
    size_t reg, i;
    uint16_t prev_val;
    size_t run;
    int16_t delta;
    uint32_t start_time, end_time;
    
    // Start CPU timing
    start_time = millis();
    
    // --- Compression Core ---
    for (reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        // Store first value uncompressed
        prev_val = buffer[0].values[reg];
        
        // Buffer space check
        if (temp_index + 2 >= MAX_COMPRESSION_SIZE) goto compression_error;
        temp[temp_index++] = (uint8_t)(prev_val >> 8);
        temp[temp_index++] = (uint8_t)(prev_val & 0xFF);

        run = 0;
        for (i = 1; i < count; i++) {
            delta = (int16_t)(buffer[i].values[reg] - prev_val);
            prev_val = buffer[i].values[reg];

            if (delta == 0) {
                run++;
                // Only encode runs of 2+ zeros (optimization)
                if (run == 255) { // Max run length reached
                    if (temp_index + 2 >= MAX_COMPRESSION_SIZE) goto compression_error;
                    temp[temp_index++] = 0x00; // Zero-run flag
                    temp[temp_index++] = (uint8_t)run;
                    run = 0;
                }
            } else {
                // Flush any pending zero run (only if meaningful)
                if (run > 1) { // Only encode runs of 2+ zeros
                    if (temp_index + 2 >= MAX_COMPRESSION_SIZE) goto compression_error;
                    temp[temp_index++] = 0x00; // Zero-run flag
                    temp[temp_index++] = (uint8_t)run;
                }
                run = 0;
                
                // Encode delta
                if (temp_index + 3 >= MAX_COMPRESSION_SIZE) goto compression_error;
                temp[temp_index++] = 0x01; // Delta flag
                temp[temp_index++] = (uint8_t)(delta >> 8);
                temp[temp_index++] = (uint8_t)(delta & 0xFF);
            }
        }
        
        // Flush final run if meaningful
        if (run > 1) {
            if (temp_index + 2 >= MAX_COMPRESSION_SIZE) goto compression_error;
            temp[temp_index++] = 0x00;
            temp[temp_index++] = (uint8_t)run;
        }
    }

    // End CPU timing
    end_time = millis();
    metrics.cpu_time_us = end_time - start_time;

    // --- Write Header (Fixed 5 bytes) ---
    if (temp_index + 5 > MAX_COMPRESSION_SIZE) goto compression_error;
    
    output[0] = (uint8_t)((count >> 8) & 0xFF);    // Sample count high
    output[1] = (uint8_t)(count & 0xFF);           // Sample count low  
    output[2] = READ_REGISTER_COUNT;               // Register count
    output[3] = (uint8_t)((temp_index >> 8) & 0xFF); // Data length high
    output[4] = (uint8_t)(temp_index & 0xFF);      // Data length low

    // --- Copy Compressed Data ---
    memcpy(output + 5, temp, temp_index);
    metrics.compressed_payload_size = 5 + temp_index; // Header + data
    
    // Calculate compression ratio
    if (metrics.original_payload_size > 0) {
        metrics.compression_ratio = (float)metrics.original_payload_size / 
                                   metrics.compressed_payload_size;
    }
    
    // --- Lossless Recovery Verification ---
    metrics.lossless_recovery_verified = verify_lossless_recovery(buffer, count, output);
    
    return metrics;

compression_error:
    metrics.compressed_payload_size = 0;
    metrics.compression_ratio = 0.0f;
    metrics.lossless_recovery_verified = false;
    return metrics;
}

/**
 * Verification function for lossless recovery
 */
bool verify_lossless_recovery(const register_reading_t* original, 
                             size_t count, 
                             const uint8_t* compressed) {
    if (compressed == NULL || original == NULL) return false;
    
    // Simple decompression for verification
    register_reading_t* decompressed = (register_reading_t*)malloc(count * sizeof(register_reading_t));
    if (decompressed == NULL) return false;
    
    // Extract header
    size_t decompressed_count = (compressed[0] << 8) | compressed[1];
    uint8_t reg_count = compressed[2];
    size_t data_length = (compressed[3] << 8) | compressed[4];
    
    if (decompressed_count != count || reg_count != READ_REGISTER_COUNT) {
        free(decompressed);
        return false;
    }
    
    const uint8_t* data = compressed + 5;
    size_t data_index = 0;
    
    // Decompress each register
    for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        if (data_index + 2 > data_length) {
            free(decompressed);
            return false;
        }
        
        uint16_t value = (data[data_index] << 8) | data[data_index + 1];
        data_index += 2;
        decompressed[0].values[reg] = value;
        
        size_t sample_index = 1;
        while (sample_index < count) {
            if (data_index >= data_length) {
                free(decompressed);
                return false;
            }
            
            uint8_t flag = data[data_index++];
            
            if (flag == 0x00) { // Zero run
                if (data_index >= data_length) {
                    free(decompressed);
                    return false;
                }
                uint8_t run_length = data[data_index++];
                for (size_t i = 0; i < run_length && sample_index < count; i++) {
                    decompressed[sample_index].values[reg] = value;
                    sample_index++;
                }
            } else if (flag == 0x01) { // Delta
                if (data_index + 1 >= data_length) {
                    free(decompressed);
                    return false;
                }
                int16_t delta = (data[data_index] << 8) | data[data_index + 1];
                data_index += 2;
                value += delta;
                decompressed[sample_index].values[reg] = value;
                sample_index++;
            } else {
                free(decompressed);
                return false; // Invalid flag
            }
        }
    }
    
    // Compare with original
    bool match = memcmp(original, decompressed, count * sizeof(register_reading_t)) == 0;
    free(decompressed);
    return match;
}