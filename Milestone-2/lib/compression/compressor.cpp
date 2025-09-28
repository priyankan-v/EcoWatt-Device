#include "compressor.h"
#include "config.h"  // For READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE

// ---------------- Compression: Raw ----------------
compression_metrics_t compress_raw(const register_reading_t* buffer, size_t count, uint8_t* output) {
    compression_metrics_t metrics = {0};
    metrics.compression_method = "Delta+RLE";
    metrics.num_samples = count;
    metrics.original_payload_size = count * READ_REGISTER_COUNT * sizeof(uint16_t);

    unsigned long start = millis();

    uint8_t temp[MAX_COMPRESSION_SIZE];
    size_t temp_index = 0;

    // Example delta+RLE compression
    for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        uint16_t prev_val = buffer[0].values[reg];
        temp[temp_index++] = (uint8_t)(prev_val >> 8);
        temp[temp_index++] = (uint8_t)(prev_val & 0xFF);

        size_t run = 0;
        for (size_t i = 1; i < count; i++) {
            int16_t delta = (int16_t)(buffer[i].values[reg] - prev_val);
            prev_val = buffer[i].values[reg];

            if (delta == 0) {
                run++;
                if (run == 255) {
                    temp[temp_index++] = 0x00;
                    temp[temp_index++] = (uint8_t)run;
                    run = 0;
                }
            } else {
                if (run > 1) {
                    temp[temp_index++] = 0x00;
                    temp[temp_index++] = (uint8_t)run;
                }
                run = 0;
                temp[temp_index++] = 0x01;
                temp[temp_index++] = (uint8_t)(delta >> 8);
                temp[temp_index++] = (uint8_t)(delta & 0xFF);
            }
        }
        if (run > 1) {
            temp[temp_index++] = 0x00;
            temp[temp_index++] = (uint8_t)run;
        }
    }

    // Header (5 bytes: count, register count, size)
    output[0] = (uint8_t)((count >> 8) & 0xFF);
    output[1] = (uint8_t)(count & 0xFF);
    output[2] = READ_REGISTER_COUNT;
    output[3] = (uint8_t)((temp_index >> 8) & 0xFF);
    output[4] = (uint8_t)(temp_index & 0xFF);

    memcpy(output + 5, temp, temp_index);

    metrics.cpu_time_ms = millis() - start;

    metrics.compressed_payload_size = 5 + temp_index;

    if (metrics.compressed_payload_size > 0) {
        metrics.compression_ratio = (float)metrics.original_payload_size / (float)metrics.compressed_payload_size;
    } else {
        metrics.compression_ratio = 0.0f;
    }
    return metrics;
}