#ifndef COMPRESS2_H
#define COMPRESS2_H

#pragma once
#include <Arduino.h>

#include "config.h"  // Include definitions like READ_REGISTER_COUNT, MEMORY_BUFFER_SIZE
#include "scheduler.h"  // Include your register_reading_t and buffers

// Max compressed size estimate: worst case (no compression)

size_t compress_buffer_with_header(const register_reading_t* buffer, size_t count, uint8_t* output);

#endif // COMPRESS2_H