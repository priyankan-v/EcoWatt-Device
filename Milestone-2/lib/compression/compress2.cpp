#include <Arduino.h>
#include "scheduler.h" // READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE

// --- Compress buffer using delta + RLE ---
size_t compress_buffer(const register_reading_t* buffer, size_t count, uint8_t* output) {
    size_t out_index = 0;

    // --- 1. Compress timestamps ---
    unsigned long prev_ts = buffer[0].timestamp;
    output[out_index++] = (prev_ts >> 24) & 0xFF;
    output[out_index++] = (prev_ts >> 16) & 0xFF;
    output[out_index++] = (prev_ts >> 8) & 0xFF;
    output[out_index++] = prev_ts & 0xFF;

    size_t run = 0;
    for (size_t i = 1; i < count; i++) {
        long delta = (long)(buffer[i].timestamp - prev_ts);
        prev_ts = buffer[i].timestamp;

        if (delta == 0) {
            run++;
            if (run == 255) {
                output[out_index++] = 0;   // zero-run marker
                output[out_index++] = run;
                run = 0;
            }
        } else {
            if (run > 0) {
                output[out_index++] = 0;
                output[out_index++] = run;
                run = 0;
            }
            output[out_index++] = 1;  // non-zero marker
            output[out_index++] = (delta >> 8) & 0xFF;
            output[out_index++] = delta & 0xFF;
        }
    }
    if (run > 0) {
        output[out_index++] = 0;
        output[out_index++] = run;
    }

    // --- 2. Compress register values ---
    for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        uint16_t prev_val = buffer[0].values[reg];
        output[out_index++] = prev_val >> 8;
        output[out_index++] = prev_val & 0xFF;

        run = 0;
        for (size_t i = 1; i < count; i++) {
            int16_t delta = buffer[i].values[reg] - prev_val;
            prev_val = buffer[i].values[reg];

            if (delta == 0) {
                run++;
                if (run == 255) {
                    output[out_index++] = 0;
                    output[out_index++] = run;
                    run = 0;
                }
            } else {
                if (run > 0) {
                    output[out_index++] = 0;
                    output[out_index++] = run;
                    run = 0;
                }
                output[out_index++] = 1;  // non-zero delta marker
                output[out_index++] = (delta >> 8) & 0xFF;
                output[out_index++] = delta & 0xFF;
            }
        }

        if (run > 0) {
            output[out_index++] = 0;
            output[out_index++] = run;
        }
    }

    return out_index;
}

// --- Decompression ---
size_t decompress_buffer(const uint8_t* input, size_t input_len, register_reading_t* output, size_t max_count) {
    size_t in_index = 0;
    size_t out_index = 0;

    // --- Timestamps ---
    unsigned long prev_ts = 0;
    prev_ts |= ((unsigned long)input[in_index++]) << 24;
    prev_ts |= ((unsigned long)input[in_index++]) << 16;
    prev_ts |= ((unsigned long)input[in_index++]) << 8;
    prev_ts |= input[in_index++];
    output[out_index++].timestamp = prev_ts;

    while (in_index < input_len && out_index < max_count) {
        uint8_t marker = input[in_index++];
        if (marker == 0) {
            uint8_t run = input[in_index++];
            for (uint8_t r = 0; r < run && out_index < max_count; r++)
                output[out_index++].timestamp = prev_ts;
        } else {
            int16_t delta = ((int16_t)input[in_index++] << 8) | input[in_index++];
            prev_ts += delta;
            output[out_index++].timestamp = prev_ts;
        }
    }

    size_t count = out_index;

    // --- Register values ---
    for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
        if (count == 0) break;

        uint16_t prev_val = ((uint16_t)input[in_index++] << 8) | input[in_index++];
        output[0].values[reg] = prev_val;

        size_t i = 1;
        while (i < count && in_index < input_len) {
            uint8_t marker = input[in_index++];
            if (marker == 0) {
                uint8_t run = input[in_index++];
                for (uint8_t r = 0; r < run && i < count; r++, i++)
                    output[i].values[reg] = prev_val;
            } else {
                int16_t delta = ((int16_t)input[in_index++] << 8) | input[in_index++];
                prev_val += delta;
                output[i].values[reg] = prev_val;
                i++;
            }
        }
    }

    return count;
}