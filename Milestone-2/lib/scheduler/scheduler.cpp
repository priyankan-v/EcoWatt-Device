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
            tasks[i].last_run_ms += tasks[i].interval_ms;
            
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
                    Serial.print(F("Current Time: "));
                    Serial.println(millis());
                    execute_upload_task();
                    Serial.print(F("Upload Task Executed at: "));
                    Serial.println(millis());
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
        Serial.println(F("Storing in Buffer 1"));

        reading = &buffer1[buffer1_count];
        reading->timestamp = epochNow();

        // Copy values to the current reading
        for (size_t i = 0; i < count; i++) {
            reading->values[i] = values[i];
        }
        
        // Fill remaining with zeros
        for (size_t i = count; i < READ_REGISTER_COUNT; i++) {
            reading->values[i] = 0;
        }

        buffer1_count++;

        if (buffer1_count >= MEMORY_BUFFER_SIZE) {
            is_buffer1_active = false;
                
            Serial.print(buffer1_count * sizeof(register_reading_t));
            Serial.println(F(" bytes"));
            Serial.println(F("Buffer 1 Data : "));
            for (size_t i = 0; i < buffer1_count; i++) {
                Serial.print(buffer1[i].timestamp);
                for (size_t j = 0; j < READ_REGISTER_COUNT; j++) {
                    Serial.print(F(" "));
                    Serial.print(buffer1[i].values[j]);
                }
                Serial.println(F("|"));
            }
            Serial.println();
        }

    } else {
        Serial.println(F("Storing in Buffer 2"));

        reading = &buffer2[buffer2_count];
        reading->timestamp = epochNow();

        // Copy values to the current reading
        for (size_t i = 0; i < count; i++) {
            reading->values[i] = values[i];
        }
        
        // Fill remaining with zeros
        for (size_t i = count; i < READ_REGISTER_COUNT; i++) {
            reading->values[i] = 0;
        }

        buffer2_count++;

        if (buffer2_count >= MEMORY_BUFFER_SIZE) {
            is_buffer1_active = true;

            Serial.print(buffer2_count * sizeof(register_reading_t));
            Serial.println(F(" bytes"));
            Serial.println(F("Buffer2 Data : "));
            for (size_t i = 0; i < buffer2_count; i++) {
                // Serial.print(F("Timestamp: "));
                Serial.print(buffer2[i].timestamp);
                for (size_t j = 0; j < READ_REGISTER_COUNT; j++) {
                    Serial.print(F(" "));
                    Serial.print(buffer2[i].values[j]);
                }
                Serial.println(F("|"));
            }
            Serial.println();
        }
    }
}


void execute_read_task(void) {
    // Serial.println(F("Executing read task..."));
    
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
            // Serial.println(F("Register values:"));
            Serial.print(F("T:"));
            Serial.print(epochNow()); // Print current epoch time
            for (size_t i = 0; i < actual_count; i++) {
                float gain = pgm_read_float(&REGISTER_GAINS[i]);
                const char* unit = (const char*)pgm_read_ptr(&REGISTER_UNITS[i]);
                float processed_value = values[i] / gain;
                
                Serial.print(F(" R"));
                Serial.print(i);
                Serial.print(F(":"));
                Serial.print(processed_value);
                // Serial.print(F(""));
                Serial.print(unit);
            }
            Serial.println();
            
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
    // Serial.println(F("Executing upload task..."));

    if(is_buffer1_active==false && buffer1_count>=MEMORY_BUFFER_SIZE)
    {
        compressed_data_len = compress_buffer_with_header(buffer1, buffer1_count, compressed_data);

        if (compressed_data_len >= 5) {
            Serial.println(F("Buffer 1 compressed successfully."));
            buffer1_count = 0; // Reset buffer1 count after preparing upload
            memset(buffer1, 0, sizeof(buffer1)); // Clear only if compression succeeded

        } else {
            log_error(ERROR_COMPRESSION_FAILED, "Buffer 1 compression failed");
            return;
        }
    }
    else if(is_buffer1_active==true && buffer2_count>=MEMORY_BUFFER_SIZE)
    {
        compressed_data_len = compress_buffer_with_header(buffer2, buffer2_count, compressed_data);

        if (compressed_data_len >= 5) {
            Serial.println(F("Buffer 2 compressed successfully."));
            buffer2_count = 0; // Reset buffer2 count after preparing upload
            memset(buffer2, 0, sizeof(buffer2)); // Clear only if compression succeeded
        } else {
            log_error(ERROR_COMPRESSION_FAILED, "Buffer 2 compression failed");
            return;
        }
    }
    else
    {
        log_error(ERROR_COMPRESSION_FAILED, "No data available for upload");
        return;
    }

    if (compressed_data_len >= 5) {
        Serial.print(F("Upload frame data ("));
        Serial.print(compressed_data_len);
        Serial.println(F(" bytes):"));

        for (size_t i = 0; i < compressed_data_len; i++) {
            Serial.print(compressed_data[i]);
            Serial.print(" ");
        }
        Serial.println();

        /*
        uint8_t encrypted_frame[MAX_COMPRESSION_SIZE];
        encrypted_frame = generate_upload_frame_from_buffer_with_encryption(compressed_data);

        uint8_t upload_frame_with_crc[MAX_COMPRESSION_SIZE+2];
        upload_frame_with_crc = append_crc_to_frame(encrypted_frame);

        String url;
        url.reserve(128);
        url = UPLOAD_API_BASE_URL;
        url += "/api/cloud/write";
        String method = "POST";
        String api_key = UPLOAD_API_KEY;
        String response = upload_api_send_request_with_retry(url, method, api_key, encrypted_frame);
        
        if (response.length() > 0) {
            if (validate_upload_response(response)) {
                    Serial.print(F("Upload successful: "));
                    Serial.print(sizeof(upload_frame_with_crc));
                    Serial.println(F(" bytes uploaded."));
                    reset_error_state();
            } else {
                log_error(ERROR_HTTP_FAILED, "Upload response validation failed");
            }
        }
        

        // memset(compressed_data, 0, sizeof(compressed_data));
        // compressed_data_len = 0;
        */

    } else {
        log_error(ERROR_COMPRESSION_FAILED, "No compressed data available for upload");
        return;
    }
}
