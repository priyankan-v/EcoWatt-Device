#include "scheduler.h"
#include "config.h"
#include "api_client.h"
#include "modbus_handler.h"
#include "error_handler.h"

// Task definitions
static scheduler_task_t tasks[TASK_COUNT] = {
    {TASK_READ_REGISTERS, POLL_INTERVAL_MS, 0, true},
    {TASK_WRITE_REGISTER, WRITE_INTERVAL_MS, 0, true},
    {TASK_HEALTH_CHECK, HEALTH_CHECK_INTERVAL_MS, 0, true}
};

// Circular buffer for readings
static register_reading_t reading_buffer[MEMORY_BUFFER_SIZE];
static size_t buffer_head = 0;
static size_t buffer_count = 0;

// PROGMEM data definitions
const PROGMEM float REGISTER_GAINS[MAX_REGISTERS] = {10.0, 10.0, 100.0, 10.0, 10.0, 10.0, 10.0, 10.0, 1.0, 1.0};
const PROGMEM char* REGISTER_UNITS[MAX_REGISTERS] = {"V", "A", "Hz", "V", "V", "A", "A", "°C", "%", "W"};
const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT] = {0x0000, 0x0001, 0x0002};

void scheduler_init(void) {
    buffer_head = 0;
    buffer_count = 0;
    Serial.println(F("Scheduler initialized"));
}

void scheduler_run(void) {
    unsigned long current_time = millis();
    
    for (int i = 0; i < TASK_COUNT; i++) {
        if (!tasks[i].enabled) {
            continue;
        }
        
        if (current_time - tasks[i].last_run_ms >= tasks[i].interval_ms) {
            tasks[i].last_run_ms = current_time;
            
            switch (tasks[i].type) {
                case TASK_READ_REGISTERS:
                    execute_read_task();
                    break;
                case TASK_WRITE_REGISTER:
                    execute_write_task();
                    break;
                case TASK_HEALTH_CHECK:
                    execute_health_check_task();
                    break;
                default:
                    break;
            }
        }
    }
    
    // Feed watchdog
    feed_watchdog();
}


void store_register_reading(const uint16_t* values, size_t count) {
    if (count > READ_REGISTER_COUNT) {
        count = READ_REGISTER_COUNT;
    }
    
    register_reading_t* reading = &reading_buffer[buffer_head];
    
    // Copy values
    for (size_t i = 0; i < count; i++) {
        reading->values[i] = values[i];
    }
    
    // Fill remaining with zeros
    for (size_t i = count; i < READ_REGISTER_COUNT; i++) {
        reading->values[i] = 0;
    }
    
    reading->timestamp = millis();
    
    // Update circular buffer indices
    buffer_head = (buffer_head + 1) % MEMORY_BUFFER_SIZE;
    if (buffer_count < MEMORY_BUFFER_SIZE) {
        buffer_count++;
    }
}

void execute_read_task(void) {
    Serial.println(F("Executing read task..."));
    
    // Generate read frame
    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_READ, 
                                       pgm_read_word(&READ_REGISTERS[0]), READ_REGISTER_COUNT);
    frame = append_crc_to_frame(frame);
    
    // Send request with retry logic
    String response = api_send_request_with_retry("read", frame);
    
    if (response.length() > 0) {
        uint16_t values[READ_REGISTER_COUNT];
        size_t actual_count;
        
        if (decode_response_registers(response, values, READ_REGISTER_COUNT, &actual_count)) {
            // Store raw values
            store_register_reading(values, actual_count);
            
            // Display processed values
            Serial.println(F("Register values:"));
            for (size_t i = 0; i < actual_count; i++) {
                float gain = pgm_read_float(&REGISTER_GAINS[i]);
                const char* unit = (const char*)pgm_read_ptr(&REGISTER_UNITS[i]);
                float processed_value = values[i] / gain;
                
                Serial.print(F("  Reg "));
                Serial.print(i);
                Serial.print(F(": "));
                Serial.print(processed_value);
                Serial.print(F(" "));
                Serial.println(unit);
            }
            
            reset_error_state();
        }
    }
}

void execute_write_task(void) {
    Serial.println(F("Executing write task..."));
    
    // Write to export power register (example: 50% power)
    uint16_t export_power_value = 50;
    
    if (!is_valid_write_value(EXPORT_POWER_REGISTER, export_power_value)) {
        log_error(ERROR_INVALID_REGISTER, "Invalid export power value");
        return;
    }
    
    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_WRITE, 
                                       EXPORT_POWER_REGISTER, export_power_value);
    frame = append_crc_to_frame(frame);
    
    String response = api_send_request_with_retry("write", frame);
    
    if (response.length() > 0) {
        if (validate_modbus_response(response)) {
            if (is_exception_response(response)) {
                uint8_t exception_code = get_exception_code(response);
                char error_msg[64];
                snprintf(error_msg, sizeof(error_msg), "Write failed with exception: 0x%02X", exception_code);
                log_error(ERROR_MODBUS_EXCEPTION, error_msg);
            } else {
                Serial.print(F("Write successful: Export power set to "));
                Serial.print(export_power_value);
                Serial.println(F("%"));
                reset_error_state();
            }
        }
    }
}

void execute_health_check_task(void) {
    check_system_health();
}
