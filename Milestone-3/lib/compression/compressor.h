#ifndef COMPRESSOR_H
#define COMPRESSOR_H
#include <Arduino.h>
#include "scheduler.h"  // Include your register_reading_t and buffers
#include "error_handler.h"  // For log_error

// Compression metrics structure for benchmark reporting
typedef struct {
    const char* compression_method;
    size_t num_samples;
    size_t original_payload_size;
    size_t compressed_payload_size;
    float compression_ratio;
    unsigned long cpu_time_ms;
} compression_metrics_t;


// Compression functions
compression_metrics_t compress_raw(const register_reading_t* buffer, size_t count, uint8_t* output);

#endif // COMPRESSOR_H