#ifndef COMPRESS2_H
#define COMPRESS2_H

#pragma once
#include <Arduino.h>

#include "config.h"  // Include definitions like READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE
#include "scheduler.h"  // Include your register_reading_t and buffers

// Max compressed size estimate: worst case (no compression)
#define MAX_COMPRESSED_SIZE (MEMORY_BUFFER_SIZE * READ_REGISTER_COUNT * sizeof(uint16_t) + MEMORY_BUFFER_SIZE * sizeof(unsigned long))

size_t compress_buffer(const register_reading_t* buffer, size_t count, uint8_t* output);
size_t decompress_buffer(const uint8_t* input, size_t input_len, register_reading_t* output, size_t max_count);

#endif // COMPRESS2_H