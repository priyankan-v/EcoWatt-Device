#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "config.h"

// Scheduler task types
typedef enum {
    TASK_READ_REGISTERS,
    TASK_WRITE_REGISTER,
    TASK_UPLOAD_DATA,
    // TASK_PERFORM_FOTA removed - now integrated into upload response
    TASK_COMMAND_HANDLING,
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

// Command state structure
typedef struct {
    bool pending;
    uint16_t register_address;
    uint16_t value;
} command_state_t;

// Scheduler functions
void scheduler_run(void);

// Buffer management functions
void allocate_buffer();
void free_buffer();
void scheduler_init();

// Data storage functions
void store_register_reading(const uint16_t* values, size_t count);

// Task execution functions
void execute_read_task(void);
void execute_write_task(void);
void execute_upload_task(void);
// execute_fota_task() removed - FOTA now triggered from upload response
void execute_command_task(void);

// Command acknowledgment functions
void send_write_command_ack(const String& status, const String& error_code = "", const String& error_message = "");

bool attempt_compression(register_reading_t* buffer, size_t* buffer_count);
size_t aggregate_buffer_avg(const register_reading_t* buffer, size_t count, register_reading_t** out_buffer);
void init_tasks_last_run(unsigned long start_time);
void finalize_command(const String& status);

#endif
