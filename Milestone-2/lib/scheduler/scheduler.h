#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "config.h"

// Scheduler task types
typedef enum {
    TASK_READ_REGISTERS,
    TASK_WRITE_REGISTER,
    TASK_UPLOAD_DATA,
    TASK_COUNT
} task_type_t;

// Task structure
typedef struct {
    task_type_t type;
    unsigned long interval_ms;
    unsigned long last_run_ms;
    bool enabled;
} scheduler_task_t;

// Circular buffer for storing readings
typedef struct {
    uint16_t values[READ_REGISTER_COUNT];
    // unsigned long timestamp;
} register_reading_t;

// Aggregated data structure for fallback when compression exceeds limits
typedef struct {
    uint16_t min_values[READ_REGISTER_COUNT];
    uint16_t max_values[READ_REGISTER_COUNT];
    uint32_t sum_values[READ_REGISTER_COUNT];
    uint16_t avg_values[READ_REGISTER_COUNT];
    uint16_t sample_count;
} aggregated_data_t;

// Scheduler functions
void scheduler_run(void);

// Data storage functions
void store_register_reading(const uint16_t* values, size_t count);

// Task execution functions
void execute_read_task(void);
void execute_write_task(void);
void execute_upload_task(void);

bool attempt_compression(register_reading_t* buffer, size_t* buffer_count);
bool create_aggregated_payload(register_reading_t* buffer, size_t count, uint8_t* output, size_t* output_len);
void calculate_aggregation(register_reading_t* buffer, size_t count, aggregated_data_t* agg_data);
void init_tasks_last_run(unsigned long start_time);

#endif
