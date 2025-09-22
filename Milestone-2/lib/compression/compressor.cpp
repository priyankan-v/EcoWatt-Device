/**
 * @file compressor.cpp
 * @brief Compresses timestamped register readings with a header for transmission.
 *
 * Uses delta and run-length encoding for timestamps and register values.
 * Adds a 5-byte header with sample count(2bytes), register count(1byte), and compressed length(2bytes).
 * Logs errors if compression exceeds buffer limits.
 *
 * Dependencies: config.h, error_handler.h
 */

#include "compressor.h"
#include "error_handler.h"  // For log_error
#include "config.h"  // For READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE

size_t compress_buffer_with_header(const register_reading_t* buffer, size_t count, uint8_t* output) {
    uint8_t temp[MAX_COMPRESSION_SIZE]; // temporary buffer for compressed data
    size_t out_index = 0;

    if (count == 0) return 0;

    // --- Compress timestamps ---
    unsigned long prev_ts = buffer[0].timestamp;
    size_t run = 0;
    size_t temp_index = 0;

    // store first timestamp absolute
    temp[temp_index++] = (prev_ts >> 24) & 0xFF;
    temp[temp_index++] = (prev_ts >> 16) & 0xFF;
    temp[temp_index++] = (prev_ts >> 8) & 0xFF;
    temp[temp_index++] = prev_ts & 0xFF;

    for (size_t i = 1; i < count; i++) {
        long delta = (long)(buffer[i].timestamp - prev_ts);
        prev_ts = buffer[i].timestamp;

        if (delta == 0) {
            run++;
            if (run == 255) {
                temp[temp_index++] = 0;
                temp[temp_index++] = run;
                run = 0;
            }
        } else {
            if (run > 0) {
                temp[temp_index++] = 0;
                temp[temp_index++] = run;
                run = 0;
            }
            temp[temp_index++] = 1;
            temp[temp_index++] = (uint8_t)((delta >> 8) & 0xFF);
            temp[temp_index++] = (uint8_t)(delta & 0xFF);
        }
    }
    if (run > 0) {
        temp[temp_index++] = 0;
        temp[temp_index++] = run;
    }

    // --- Compress register values ---
    for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        uint16_t prev_val = buffer[0].values[reg];
        temp[temp_index++] = (uint8_t)(prev_val >> 8);
        temp[temp_index++] = (uint8_t)(prev_val & 0xFF);

        run = 0;
        for (size_t i = 1; i < count; i++) {
            int16_t delta = (int16_t)(buffer[i].values[reg] - prev_val);
            prev_val = buffer[i].values[reg];

            if (delta == 0) {
                run++;
                if (run == 255) {
                    temp[temp_index++] = 0;
                    temp[temp_index++] = run;
                    run = 0;
                }
            } else {
                if (run > 0) {
                    temp[temp_index++] = 0;
                    temp[temp_index++] = run;
                    run = 0;
                }
                temp[temp_index++] = 1;
                temp[temp_index++] = (uint8_t)((delta >> 8) & 0xFF);
                temp[temp_index++] = (uint8_t)(delta & 0xFF);
            }
        }
        if (run > 0) {
            temp[temp_index++] = 0;
            temp[temp_index++] = run;
        }
    }

      if (temp_index + 5 > MAX_COMPRESSION_SIZE) {
        log_error(ERROR_COMPRESSION_FAILED, "Compression overflowed");
        return 0;
    }

    // --- Write header ---
    out_index = 0;
    output[out_index++] = (count >> 8) & 0xFF;              // buffer count high
    output[out_index++] = (count & 0xFF);                  // buffer count low
    output[out_index++] = READ_REGISTER_COUNT;             // register count
    output[out_index++] = (temp_index >> 8) & 0xFF;        // compressed length high
    output[out_index++] = (temp_index & 0xFF);             // compressed length low

    if (out_index > 5) {
        log_error(ERROR_COMPRESSION_FAILED, "Header compression buffer overflow");
        return 0;
    }

    // --- Copy compressed payload ---
    memcpy(output + out_index, temp, temp_index);
    out_index += temp_index;

    return out_index; // total bytes including header
}