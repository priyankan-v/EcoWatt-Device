#ifndef COMPRESSOR_H
#define COMPRESSOR_H
#include <Arduino.h>
#include "scheduler.h"  // Include your register_reading_t and buffers

// Compression metrics structure for benchmark reporting
typedef struct {
    const char* compression_method;
    size_t num_samples;
    size_t original_payload_size;
    size_t compressed_payload_size;
    float compression_ratio;
    uint32_t cpu_time_us;
    bool lossless_recovery_verified;
} compression_metrics_t;

// Compression functions
compression_metrics_t compress_with_benchmark(const register_reading_t* buffer, size_t count, uint8_t* output);
bool verify_lossless_recovery(const register_reading_t* original, size_t count, const uint8_t* compressed);


#endif // COMPRESSOR_H