#include "scheduler.h"
#include "config.h"
#include "api_client.h"
#include "modbus_handler.h"
#include "error_handler.h"
#include "cloudAPI_handler.h"
#include "time_sync.h"
#include "compress2.h"

// Task definitions
static scheduler_task_t tasks[TASK_COUNT] = {
    {TASK_READ_REGISTERS, POLL_INTERVAL_MS, 0, true},
    {TASK_WRITE_REGISTER, WRITE_INTERVAL_MS, 0, true},
    {TASK_HEALTH_CHECK, HEALTH_CHECK_INTERVAL_MS, 0, true},
    {TASK_UPLOAD_DATA, UPLOAD_INTERVAL_MS, 0, true}
};

// Double buffer definitions
static register_reading_t buffer1[MEMORY_BUFFER_SIZE];
static register_reading_t buffer2[MEMORY_BUFFER_SIZE];
static size_t buffer1_head = 0, buffer2_head = 0;
static size_t buffer1_count = 0, buffer2_count = 0;
static bool is_buffer1_active = true;

uint8_t compressed_data[MAX_COMPRESSION_SIZE] = {0}; // Output buffer for compression
uint16_t compressed_data_len = 0; // Length of compressed data
static unsigned long last_buffer_full_time = 0; // Timestamp of last buffer full event

// PROGMEM data definitions
const PROGMEM float REGISTER_GAINS[MAX_REGISTERS] = {10.0, 10.0, 100.0, 10.0, 10.0, 10.0, 10.0, 10.0, 1.0, 1.0};
const PROGMEM char* REGISTER_UNITS[MAX_REGISTERS] = {"V", "A", "Hz", "V", "V", "A", "A", "°C", "%", "W"};
const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009};

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
                case TASK_UPLOAD_DATA:
                    execute_upload_task();
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

    register_reading_t* reading;

    if (is_buffer1_active) {
        if (buffer1_count >= MEMORY_BUFFER_SIZE) {
            unsigned long time_after_buffer_full = millis();

            if ((time_after_buffer_full - last_buffer_full_time) >= UPLOAD_INTERVAL_MS) {
                last_buffer_full_time = time_after_buffer_full;
                compressed_data_len = compress_buffer(buffer1, buffer1_count, compressed_data);

                if (compressed_data_len > 0) {
                    Serial.println(F("Buffer 1 compressed successfully."));
                } else {
                    log_error(ERROR_COMPRESSION_FAILED, "Buffer 1 compression failed");
                }
            }

            if (compressed_data_len > 0) {
                memset(buffer1, 0, sizeof(buffer1)); // Clear only if compression succeeded
            }

            buffer1_head = 0;
            buffer1_count = 0;
            is_buffer1_active = false;
        }

        reading = &buffer1[buffer1_head];
        buffer1_head = (buffer1_head + 1) % MEMORY_BUFFER_SIZE;
        buffer1_count++;

    } else {

        if (buffer2_count >= MEMORY_BUFFER_SIZE) {
            unsigned long time_after_buffer_full = millis();

            if ((time_after_buffer_full - last_buffer_full_time) >= UPLOAD_INTERVAL_MS) {
                last_buffer_full_time = time_after_buffer_full;
                compressed_data_len = compress_buffer(buffer2, buffer2_count, compressed_data); // Just to estimate size
                memset(buffer2, 0, sizeof(buffer2));

            }

            buffer2_head = 0;
            buffer2_count = 0;
            is_buffer1_active = true;
        }

        reading = &buffer2[buffer2_head];
        buffer2_head = (buffer2_head + 1) % MEMORY_BUFFER_SIZE;
        buffer2_count++;
    }

    // Copy values to the current reading
    for (size_t i = 0; i < count; i++) {
        reading->values[i] = values[i];
    }
    
    // Fill remaining with zeros
    for (size_t i = count; i < READ_REGISTER_COUNT; i++) {
        reading->values[i] = 0;
    }

    reading->timestamp = epochNow();
}

void execute_read_task(void) {
    Serial.println(F("Executing read task..."));
    
    // Generate read frame
    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_READ, 
                                       pgm_read_word(&READ_REGISTERS[0]), READ_REGISTER_COUNT);
    frame = append_crc_to_frame(frame);
    
    String url;
    url.reserve(128);
    url = API_BASE_URL;
    url += "/api/inverter/read";
    String method = "POST";
    String api_key = API_KEY;
    String response = api_send_request_with_retry(url, method, api_key, frame);
    
    if (response.length() > 0) {
        uint16_t values[READ_REGISTER_COUNT];
        size_t actual_count;
        
        if (decode_response_registers(response, values, READ_REGISTER_COUNT, &actual_count)) {
            // Store raw values
            store_register_reading(values, actual_count);
            
            // Display processed values
            Serial.println(F("Register values:"));
            Serial.println(epochNow()); // Print current epoch time
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
    

    String url;
    url.reserve(128);
    url = API_BASE_URL;
    url += "/api/inverter/write";
    String method = "POST";
    String api_key = API_KEY;
    String response = api_send_request_with_retry(url, method, api_key, frame);
    
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


void execute_upload_task(void) {
    Serial.println(F("Executing upload task..."));
    String frame;
    frame.reserve(MAX_COMPRESSION_SIZE * 2);

    if (compressed_data_len > 0) {
        frame = bytes_to_hex(compressed_data, compressed_data_len);
        Serial.print(F("Compressed data: "));
        Serial.println(frame);
    } else {
        log_error(ERROR_COMPRESSION_FAILED, "No compressed data available for upload");
        return;
    }

    String encrypted_frame = generate_upload_frame_from_buffer_with_encryption(frame);
    encrypted_frame = append_crc_to_frame(encrypted_frame);

    String url;
    url.reserve(128);
    url = UPLOAD_API_BASE_URL;
    url += "/api/cloud/write";
    String method = "POST";
    String api_key = UPLOAD_API_KEY;
    String response = api_send_request_with_retry(url, method, api_key, encrypted_frame);
    
    if (response.length() > 0) {
        if (validate_upload_response(response)) {
                Serial.print(F("Upload successful: "));
                Serial.print(encrypted_frame.length());
                Serial.println(F(" bytes uploaded."));
                reset_error_state();
        } else {
            log_error(ERROR_HTTP_FAILED, "Upload response validation failed");
        }
    }
}

// Convert a byte array to a hex string
String bytes_to_hex(const uint8_t* data, size_t len) {
    String hex;
    hex.reserve(len * 2); // pre-allocate for efficiency

    const char hex_chars[] = "0123456789ABCDEF";

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        hex += hex_chars[byte >> 4];   // high nibble
        hex += hex_chars[byte & 0x0F]; // low nibble
    }

    return hex;
}