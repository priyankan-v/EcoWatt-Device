#ifndef COMPRESS2_H
#define COMPRESS2_H
#include <Arduino.h>
#include "scheduler.h"  // Include your register_reading_t and buffers


// Max compressed size estimate: worst case (no compression)
size_t compress_buffer_with_header(const register_reading_t* buffer, size_t count, uint8_t* output);

#endif // COMPRESS2_H