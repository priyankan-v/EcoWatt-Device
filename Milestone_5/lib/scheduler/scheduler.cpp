#include "scheduler.h"
#include "config.h"
#include "config_manager.h"
#include "api_client.h"
#include "modbus_handler.h"
#include "error_handler.h"
#include "cloudAPI_handler.h"
#include "compressor.h"
#include "fota.h"
#include "encryptionAndSecurity.h"
#include "esp_task_wdt.h"
#include "command_parse.h"
#include "time_utils.h"
#include "wifi_manager.h"


extern NonceManager nonceManager; // Declare the global instance from main.cpp

// Task definitions
static scheduler_task_t tasks[TASK_COUNT] = {
    {TASK_READ_REGISTERS, POLL_INTERVAL_MS, 0, true},
    {TASK_WRITE_REGISTER, WRITE_INTERVAL_MS, 0, false},
    {TASK_UPLOAD_DATA, UPLOAD_INTERVAL_MS, 0, true},
    // FOTA task removed - now integrated into upload response handling
    {TASK_COMMAND_HANDLING, COMMAND_INTERVAL_MS, 0, false}
};

// Dynamic buffer definition - Buffer Rules Implementation
static register_reading_t* buffer = nullptr;  // Dynamic buffer allocated based on config
static size_t buffer_count = 0;
static size_t buffer_write_index = 0;  // For circular buffer behavior
static bool upload_in_progress = false;  // Prevents filling during upload
static bool buffer_full = false;  // Tracks if buffer is full
static size_t buffer_size = 0;  // Current allocated buffer size
static uint32_t last_upload_interval = 0;  // Track config changes
static uint32_t last_sampling_interval = 0;  // Track config changes
static unsigned long last_upload_attempt = 0;  // For retry delays
static int upload_retry_count = 0;  // Track retry attempts

// Write command tracking
static command_state_t current_command = {false, 0, 0};
static String write_status = ""; // Track if last write was successful
static String write_executed_timestamp = ""; // Timestamp of last write execution

uint8_t compressed_data[MAX_COMPRESSION_SIZE] = {0}; // Output buffer for compression
size_t compressed_data_len = 0; // Length of compressed data
compression_metrics_t compression_metrics = {0}; // Metrics of last compression

// Internal buffer allocation with specific size
static bool allocate_buffer_internal(size_t new_size) {
    if (new_size == 0) {
        Serial.println(F("[BUFFER] Cannot allocate buffer with size 0"));
        return false;
    }
    
    // Free existing buffer if it exists
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
    
    // Allocate new buffer
    buffer = (register_reading_t*)malloc(new_size * sizeof(register_reading_t));
    if (buffer == nullptr) {
        Serial.printf("[BUFFER] ERROR: Failed to allocate %zu bytes for buffer\n", 
                     new_size * sizeof(register_reading_t));
        buffer_size = 0;
        return false;
    }
    
    // Initialize buffer to zero
    memset(buffer, 0, new_size * sizeof(register_reading_t));
    buffer_size = new_size;
    buffer_count = 0;
    buffer_write_index = 0;
    buffer_full = false;
    
    Serial.printf("[BUFFER] Allocated dynamic buffer: %zu samples (%zu bytes)\n", 
                 buffer_size, buffer_size * sizeof(register_reading_t));
    return true;
}

// Public buffer allocation function (calculates size from config)
void allocate_buffer() {
    if (!g_config_manager || !g_config_manager->is_initialized()) {
        Serial.println(F("[BUFFER] Config manager not initialized, using default size"));
        allocate_buffer_internal(MEMORY_BUFFER_SIZE);
        return;
    }
    
    uint32_t upload_interval = config_get_upload_interval_ms();
    uint32_t sampling_interval = config_get_sampling_interval_ms();
    
    if (upload_interval == 0 || sampling_interval == 0) {
        Serial.println(F("[BUFFER] Invalid intervals, using default buffer size"));
        allocate_buffer_internal(MEMORY_BUFFER_SIZE);
        return;
    }
    
    size_t calculated_buffer_size = (upload_interval / sampling_interval) + 1;
    
    // Enforce reasonable limits
    if (calculated_buffer_size < 5) {
        calculated_buffer_size = 5;
    } else if (calculated_buffer_size > 100) {
        calculated_buffer_size = 100;
    }
    
    Serial.printf("[BUFFER] Calculating buffer size: %ums / %ums + 1 = %zu samples\n", 
                 upload_interval, sampling_interval, calculated_buffer_size);
    
    allocate_buffer_internal(calculated_buffer_size);
}

void free_buffer() {
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
        buffer_size = 0;
        buffer_count = 0;
        buffer_write_index = 0;
        buffer_full = false;
        Serial.println(F("[BUFFER] Dynamic buffer freed"));
    }
}

// Initialization function
void scheduler_init() {
    Serial.println("[SCHEDULER] Initializing scheduler with dynamic buffer...");
    
    // Allocate initial buffer based on current configuration
    allocate_buffer();
    
    Serial.println("[SCHEDULER] Scheduler initialization complete");
}

// PROGMEM data definitions
const PROGMEM float REGISTER_GAINS[MAX_REGISTERS] = {10.0, 10.0, 100.0, 10.0, 10.0, 10.0, 10.0, 10.0, 1.0, 1.0};
const PROGMEM char* REGISTER_UNITS[MAX_REGISTERS] = {"V", "A", "Hz", "V", "V", "A", "A", "°C", "%", "W"};
const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT] = {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009};

void scheduler_run(void) {
    unsigned long current_time = millis();
    
    // Update task intervals from ConfigManager if available
    if (g_config_manager && g_config_manager->is_initialized()) {
        tasks[TASK_READ_REGISTERS].interval_ms = config_get_sampling_interval_ms();
        tasks[TASK_UPLOAD_DATA].interval_ms = config_get_upload_interval_ms();
        // Couple command interval to upload interval for synchronized timing
        tasks[TASK_COMMAND_HANDLING].interval_ms = config_get_upload_interval_ms();
        
        // Recalculate buffer size only when configuration changes
        uint32_t upload_interval = config_get_upload_interval_ms();
        uint32_t sampling_interval = config_get_sampling_interval_ms();
        
        if (upload_interval != last_upload_interval || sampling_interval != last_sampling_interval || buffer == nullptr) {
            // Configuration changed or buffer not allocated - reallocate buffer
            Serial.printf("[BUFFER] Config changed: upload %u->%u, sampling %u->%u\n", 
                         last_upload_interval, upload_interval, last_sampling_interval, sampling_interval);
            
            size_t calculated_buffer_size = (upload_interval / sampling_interval) + 2; // +2 for safety margin
            
            // Set reasonable limits
            if (calculated_buffer_size < 5) calculated_buffer_size = 5;   // Minimum 5 samples
            if (calculated_buffer_size > 100) calculated_buffer_size = 100; // Maximum 100 samples
            
            Serial.printf("[BUFFER] Calculation: %u / %u + 2 = %zu\n", 
                         upload_interval, sampling_interval, calculated_buffer_size);
            
            // Reallocate buffer with new size
            if (allocate_buffer_internal(calculated_buffer_size)) {
                last_upload_interval = upload_interval;
                last_sampling_interval = sampling_interval;
                Serial.printf("[BUFFER] Dynamic buffer allocated: %zu samples (upload: %us, sampling: %us)\n\r", 
                             buffer_size, upload_interval/1000, sampling_interval/1000);
            } else {
                Serial.println(F("[BUFFER] ERROR: Failed to allocate dynamic buffer, using fallback"));
                // Keep old values to prevent infinite reallocation attempts
            }
        }
    }

    for (int i = 0; i < TASK_COUNT; i++) {
        if (!tasks[i].enabled) {
            continue;
        }
        
        if (current_time - tasks[i].last_run_ms >= tasks[i].interval_ms) {
            // Update last_run_ms BEFORE executing task to prevent re-triggering
            tasks[i].last_run_ms = current_time;
            
            switch (tasks[i].type) {
                case TASK_READ_REGISTERS:
                    execute_read_task();
                    if (POWER_MANAGMENT) {
                        current_time = millis();
                        unsigned long read_slack = tasks[i].interval_ms - (current_time - tasks[i].last_run_ms);
                        unsigned long upload_slack = tasks[2].interval_ms - (current_time - tasks[2].last_run_ms);
                        if (read_slack > 0 && upload_slack > 0) {
                            if (read_slack < upload_slack) {
                                if (LIGHT_SLEEP) {
                                    esp_err_t timer_result = esp_sleep_enable_timer_wakeup(read_slack * 1000); // micro_seconds
                                    if (timer_result == ESP_OK){
                                        Serial.println("Timer Success");
                                    };
                                    Serial.flush();
                                    esp_err_t wakeup_result = esp_light_sleep_start(); 
                                    if (wakeup_result == ESP_OK) {
                                        Serial.printf("Read Slack: %lu\n\r", read_slack);
                                        unsigned long wakeup_time = millis();
                                        Serial.printf("Light Sleep Time: %lu\n\r", wakeup_time - current_time);
                                        Serial.begin(SERIAL_BAUD_RATE);
                                        wifi_init();
                                        Serial.printf("Wifi Reconnection Time: %lu\n\r", millis() - wakeup_time);
                                    };
                                } else if (IDLE_DELAY) {
                                    delay(read_slack);
                                };
                            } else {
                                if (LIGHT_SLEEP) {
                                    esp_sleep_enable_timer_wakeup(upload_slack * 1000); // micro_seconds
                                    Serial.flush();
                                    esp_light_sleep_start();
                                    Serial.begin(SERIAL_BAUD_RATE);
                                    wifi_init();
                                } else if (IDLE_DELAY) {
                                    delay(upload_slack);
                                };
                            };                        
                        };
                    };
                    break;
                case TASK_COMMAND_HANDLING:
                    execute_command_task();
                    break;
                case TASK_UPLOAD_DATA:
                    execute_upload_task();
                    if (POWER_MANAGMENT) {
                        current_time = millis();
                        unsigned long read_slack = tasks[i].interval_ms - (current_time - tasks[i].last_run_ms);
                        if (read_slack > 0) {
                            if (LIGHT_SLEEP) {
                                esp_sleep_enable_timer_wakeup(read_slack * 1000); // micro_seconds
                                Serial.flush();
                                esp_light_sleep_start();
                                Serial.begin(SERIAL_BAUD_RATE);
                                wifi_init();
                            } else if (IDLE_DELAY) {
                                delay(read_slack);
                            };                     
                        };
                    };
                    break;
                // FOTA task removed - now handled in upload response
                // WRITE task removed - now executes immediately when command received
                default:
                    break;
            }
        }
    }
    
    // Feed watchdog
    feed_watchdog();
}


void store_register_reading(const uint16_t* values, size_t count) {
    // Check if buffer is allocated
    if (buffer == nullptr || buffer_size == 0) {
        Serial.println(F("[BUFFER] ERROR: Buffer not allocated, skipping sample"));
        return;
    }
    
    // Always stop filling buffer during upload (workflow requirement)
    if (upload_in_progress) {
        Serial.println(F("[BUFFER] Skipping sample - upload in progress"));
        return;
    }
    
    // Check buffer full behavior when not uploading
    if (buffer_full) {
        #if BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_STOP
            Serial.println(F("[BUFFER] Buffer full - stopping new acquisitions until upload"));
            return;
        #elif BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_CIRCULAR
            Serial.println(F("[BUFFER] Buffer full - overwriting oldest data (circular buffer)"));
            // Continue with circular buffer behavior
        #endif
    }
    
    if (count > READ_REGISTER_COUNT) {
        count = READ_REGISTER_COUNT;
    }

    register_reading_t* reading = &buffer[buffer_write_index];

    // Copy values to the current reading
    for (size_t i = 0; i < count; i++) {
        reading->values[i] = values[i];
    }
    
    // Fill remaining with zeros
    for (size_t i = count; i < READ_REGISTER_COUNT; i++) {
        reading->values[i] = 0;
    }

    // Advance write index (circular buffer)
    buffer_write_index = (buffer_write_index + 1) % buffer_size;

    // Track buffer usage
    if (!buffer_full) {
        buffer_count++;
        if (buffer_count >= buffer_size) {
            buffer_full = true;
            #if BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_CIRCULAR
                Serial.println(F("[BUFFER] Buffer full - using circular overwrite"));
            #elif BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_STOP
                Serial.println(F("[BUFFER] Buffer full - will stop acquisitions"));
            #endif
        }
    }

    if (buffer_count % buffer_size == 0 || buffer_full) {  // Log every buffer_size samples or when full
        Serial.print(F("[BUFFER] Samples: "));
        Serial.print(buffer_count);
        Serial.print(F("/"));
        Serial.print(buffer_size);
        Serial.print(F(" (write_index: "));
        Serial.print(buffer_write_index);
        #if BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_CIRCULAR
            Serial.println(F(", behavior: CIRCULAR"));
        #elif BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_STOP
            Serial.println(F(", behavior: STOP"));
        #endif
        // Serial.println(F(")"));
        // for (size_t i = 0; i < buffer_count; i++) {
        //         for (size_t j = 0; j < READ_REGISTER_COUNT; j++) {
        //             Serial.print(buffer[i].values[j]);
        //             Serial.print(F(" "));
        //         }
        //         Serial.println(F("|"));
        //     }
    }
}


void execute_read_task(void) {
    Serial.println(F("Executing read task..."));
    
    // Get current configuration
    uint8_t slave_addr = config_get_slave_address();
    uint8_t register_count = config_get_register_count();
    uint16_t active_registers[MAX_REGISTERS];
    config_get_active_registers(active_registers, MAX_REGISTERS);
    
    // Use configured registers or fall back to default
    uint16_t start_register = (register_count > 0) ? active_registers[0] : pgm_read_word(&READ_REGISTERS[0]);
    
    // Generate read frame
    String frame = format_request_frame(slave_addr, FUNCTION_CODE_READ, start_register, register_count);
    frame = append_crc_to_frame(frame);
    
    String url;
    url.reserve(128);
    url = API_BASE_URL;
    url += "/api/inverter/read";
    String method = "POST";
    String api_key = API_KEY;
    String response = api_send_request_with_retry(url, method, api_key, frame);
    
    if (response.length() > 0) {
        uint16_t read_values[READ_REGISTER_COUNT];
        size_t actual_count;

        if (decode_response_registers(response, read_values, READ_REGISTER_COUNT, &actual_count)) {
            // Store raw values
            store_register_reading(read_values, actual_count);
            
            // Display processed values
            for (size_t i = 0; i < actual_count; i++) {
                float gain = pgm_read_float(&REGISTER_GAINS[i]);
                const char* unit = (const char*)pgm_read_ptr(&REGISTER_UNITS[i]);
                float processed_value = read_values[i] / gain;
                
                Serial.print(F("R"));
                Serial.print(i);
                Serial.print(F(":"));
                Serial.print(processed_value);
                Serial.print(unit);
                Serial.print(F(" "));
            }
            Serial.println();
            
            reset_error_state();
        }
    }
}

void execute_write_task(void) {
    Serial.println(F("Executing write task..."));
    
    if (!current_command.pending) {
        Serial.println(F("[WRITE] No pending command - skipping"));
        tasks[TASK_WRITE_REGISTER].enabled = false;
        return;
    }
    
    uint16_t export_power_value = current_command.value;
    uint16_t target_register = current_command.register_address;
    
    if (!is_valid_write_value(target_register, export_power_value)) {
        log_error(ERROR_INVALID_REGISTER, "Invalid export power value");
        finalize_command("Failed - Invalid value");
        return;
    }

    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_WRITE, target_register, export_power_value);

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
                finalize_command("Failed - Exception");
            } else {
                Serial.print(F("Write successful: Register "));
                Serial.print(target_register);
                Serial.print(F(" set to "));
                Serial.println(export_power_value);
                finalize_command("Success");
            }
        } else {
            finalize_command("Failed - Invalid response");
        }
    } else {
        finalize_command("Failed - No response");
    }
}

void execute_upload_task(void) {
    upload_in_progress = true;  // Prevent buffer filling during upload

    bool use_aggregation = false;
    
    // Check if we have data to upload
    if (buffer_count == 0) {
        Serial.println(F("[COMPRESSION] No data to compress and upload"));
        upload_in_progress = false;  // Re-enable filling
        return;
    }
    
    // Check retry delay if previous upload failed
    unsigned long current_time = millis();
    if (upload_retry_count > 0) {
        unsigned long retry_delay = RETRY_BASE_DELAY_MS * (1 << (upload_retry_count - 1));
        if (retry_delay > MAX_RETRY_DELAY_MS) retry_delay = MAX_RETRY_DELAY_MS;
        
        if (current_time - last_upload_attempt < retry_delay) {
            Serial.print(F("[UPLOAD] Waiting for retry delay: "));
            Serial.print((retry_delay - (current_time - last_upload_attempt)) / 1000);
            Serial.println(F("s remaining"));
            return;
        }
    }
    
    Serial.print(F("[UPLOAD] Starting upload - Buffer has "));
    Serial.print(buffer_count);
    Serial.println(F(" samples"));
    
    // WORKFLOW STEP 1: Stop filling → finalize buffer
    upload_in_progress = true;
    Serial.println(F("[WORKFLOW] Stop filling → finalize buffer"));
    
    // WORKFLOW STEP 2: Compress + packetize
    Serial.println(F("[WORKFLOW] Compress + packetize"));

    if (!attempt_compression(buffer, &buffer_count)) {
        memset(compressed_data, 0, sizeof(compressed_data));
        memset(&compression_metrics, 0, sizeof(compression_metrics));
        compressed_data_len = 0;
        upload_retry_count++;
        last_upload_attempt = current_time;
        Serial.print(F("[UPLOAD] Compression failed - retry count: "));
        Serial.println(upload_retry_count);
        upload_in_progress = false;  // Re-enable filling on failure
        return;
    }
    
    // Check if compressed data exceeds payload limit
    if (compressed_data_len > MAX_PAYLOAD_SIZE) {
        Serial.print(F("Compressed data ("));
        Serial.print(compressed_data_len);
        Serial.print(F(" bytes) exceeds limit ("));
        Serial.print(MAX_PAYLOAD_SIZE);
        Serial.println(F(" bytes). Using aggregation..."));
        use_aggregation = true;
        
        size_t aggregated_count = 0;
        register_reading_t* aggregated_buffer = NULL;
        aggregated_count = aggregate_buffer_avg(buffer, buffer_count, &aggregated_buffer);

        if (!attempt_compression(aggregated_buffer, &aggregated_count)) {
            memset(&compression_metrics, 0, sizeof(compression_metrics));
            memset(compressed_data, 0, sizeof(compressed_data));
            compressed_data_len = 0;
            upload_retry_count++;
            last_upload_attempt = current_time;
            Serial.print(F("[UPLOAD] Aggregated Compression failed - retry count: "));
            Serial.println(upload_retry_count);
            free(aggregated_buffer);
            upload_in_progress = false;  // Re-enable filling on failure
            return;
        }
        
        free(aggregated_buffer);
    }

    if (compressed_data_len >= 5 && compressed_data_len <= MAX_PAYLOAD_SIZE) {
        
        Serial.print(F("[UPLOAD] Method: "));
        Serial.print(use_aggregation ? F("AGGREGATED COMPRESSION") : F("RAW COMPRESSION"));
        Serial.print(F(", Original: "));
        Serial.print(compression_metrics.original_payload_size);
        Serial.print(F(" bytes, Final: "));
        Serial.print(compression_metrics.compressed_payload_size);
        Serial.print(F(" bytes, Ratio: "));
        Serial.println(compression_metrics.compression_ratio);

        // Create final upload frame: [metadata][compressed_data]
        uint8_t compressed_data_frame[compressed_data_len + 1];
        if (use_aggregation) {
            // Indicate aggregated in header
            compressed_data_frame[0] = 0x01; // Aggregated flag
        } else {
            compressed_data_frame[0] = 0x00; // Raw flag
        }
        
        // Copy metadata
        memcpy(compressed_data_frame + 1, compressed_data, compressed_data_len);

        Serial.println(F("[UPLOAD] Compressed data frame:"));
        for (size_t i = 0; i < compressed_data_len + 1; i++) {
            Serial.print(compressed_data_frame[i]);
            Serial.print(F(" "));
        }
        Serial.println();
        
        // Add CRC for entire frame
        uint8_t upload_frame_with_crc[compressed_data_len + 3]; // metadata + data + CRC
        append_crc_to_upload_frame(compressed_data_frame, compressed_data_len + 1, upload_frame_with_crc);
        
        Serial.print(F("[UPLOAD] Frame with CRC: "));
        Serial.print(compressed_data_len + 1);
        Serial.print(F(" bytes + 2 bytes CRC = "));
        Serial.print(compressed_data_len + 3);
        Serial.println(F(" bytes total"));
        
        // === AES-256-CBC ENCRYPTION ===
        uint8_t iv[16]; // 16-byte IV for AES
        uint8_t encrypted_payload[compressed_data_len + 32]; // Extra space for PKCS#7 padding
        size_t encrypted_len = 0;
        
        Serial.println(F("[ENCRYPTION] Encrypting payload with AES-256-CBC..."));
        
        extern bool encryptPayloadAES_CBC(const uint8_t* plaintext, size_t plaintext_len,
                                         uint8_t* ciphertext, size_t* ciphertext_len,
                                         uint8_t* iv_output);
        
        if (!encryptPayloadAES_CBC(upload_frame_with_crc, compressed_data_len + 3,
                                  encrypted_payload, &encrypted_len, iv)) {
            Serial.println(F("[ENCRYPTION] Encryption failed! Aborting upload."));
            upload_in_progress = false;
            return;
        }
        
        // Combine IV + Ciphertext for final payload
        uint8_t final_payload[16 + encrypted_len];
        memcpy(final_payload, iv, 16); // First 16 bytes: IV
        memcpy(final_payload + 16, encrypted_payload, encrypted_len); // Rest: Ciphertext
        size_t final_payload_len = 16 + encrypted_len;
        
        Serial.printf("[ENCRYPTION] Final encrypted payload: IV(16) + Ciphertext(%d) = %d bytes\n",
                     encrypted_len, final_payload_len);

        String url;
        url.reserve(128);
        url = UPLOAD_API_BASE_URL;
        url += "/api/cloud/write";
        String method = "POST";
        String api_key = UPLOAD_API_KEY;

        // Get a unique nonce for this transaction
        uint32_t nonce = nonceManager.getAndIncrementNonce();
        Serial.print(F("[SECURITY] Using Nonce: "));
        Serial.println(nonce);

        // Encode encrypted payload (IV + Ciphertext) to Base64
        String upload_frame_base64 = encodeBase64(final_payload, final_payload_len);
        Serial.print(F("[SECURITY] Base64 encoded length: "));
        Serial.println(upload_frame_base64.length());

        // Generate MAC for the encrypted Base64 payload
        String mac = generateMAC(upload_frame_base64);
        Serial.print(F("[SECURITY] Generated MAC: "));
        Serial.println(mac);

        String response = upload_api_send_request_with_retry(url, method, api_key, final_payload, final_payload_len, String(nonce), mac);

        if (response.length() > 0) {
            String action;
            uint16_t reg = 0;
            uint16_t val = 0;

            if (extract_command(response, action, reg, val)) {
                Serial.println(F("[COMMAND] Command detected in cloud response"));
                
                if (action.equalsIgnoreCase("write_register")) {
                    Serial.println(F("[COMMAND] Executing WRITE command immediately"));

                    // Store command atomically
                    current_command.pending = true;
                    current_command.register_address = reg;
                    current_command.value = val;
                    
                    // Execute write immediately (no need to wait for scheduler interval)
                    execute_write_task();
                    
                    // Command task will report result on next interval
                    tasks[TASK_COMMAND_HANDLING].enabled = true;

                } else if (action.equalsIgnoreCase("read_register")) {
                    Serial.println(F("[COMMAND] Preparing to execute READ task"));

                } else {
                    Serial.println(F("[COMMAND] Unknown action command received"));
                }
            }
        }
        
        if (validate_upload_response(response)) {
            Serial.print(F("[UPLOAD] Success: "));
            Serial.print(compressed_data_len + 3);
            Serial.println(F(" bytes uploaded"));
            
            // STEP 1: Process configuration updates from cloud response
            String config_ack = config_process_cloud_response(response);
            if (config_ack.length() > 0) {
                // Send configuration acknowledgment to cloud
                extern void send_config_ack_to_cloud(const String& ack_json);
                send_config_ack_to_cloud(config_ack);
                // Note: ACK failure doesn't prevent config application
            }
            
            // STEP 2: Apply any pending configuration changes after successful upload
            if (config_has_pending_changes()) {
                Serial.println(F("[CONFIG] Applying pending configuration changes"));
                
                // Feed watchdog before potentially blocking operation
                esp_task_wdt_reset();
                
                // Apply with timeout protection
                bool apply_success = false;
                unsigned long apply_start = millis();
                const unsigned long APPLY_TIMEOUT = 5000; // 5 seconds max
                
                try {
                    config_apply_pending_changes();
                    apply_success = true;
                    Serial.println(F("[CONFIG] Configuration applied successfully"));
                } catch (...) {
                    Serial.println(F("[CONFIG] ERROR: Exception during config application"));
                }
                
                // Check for timeout
                if (millis() - apply_start > APPLY_TIMEOUT) {
                    Serial.println(F("[CONFIG] WARNING: Config application took too long"));
                }
                
                // Feed watchdog after config operation
                esp_task_wdt_reset();
                
                if (!apply_success) {
                    Serial.println(F("[CONFIG] ERROR: Failed to apply configuration changes"));
                    // Clear pending config to prevent retry loops
                    config_clear_pending_changes();
                }
            }
            
            // STEP 3: Check for FOTA manifest in cloud response
            int job_id;
            String fwUrl, shaExpected, signature;
            size_t fwSize;
            
            extern bool parse_fota_manifest_from_response(const String& response, 
                                                         int& job_id, 
                                                         String& fwUrl, 
                                                         size_t& fwSize, 
                                                         String& shaExpected, 
                                                         String& signature);
            
            if (parse_fota_manifest_from_response(response, job_id, fwUrl, fwSize, shaExpected, signature)) {
                Serial.println(F("[FOTA] Firmware update available - initiating download"));
                
                extern bool perform_FOTA_with_manifest(int job_id, 
                                                      const String& fwUrl, 
                                                      size_t fwSize, 
                                                      const String& shaExpected, 
                                                      const String& signature);
                
                bool fota_success = perform_FOTA_with_manifest(job_id, fwUrl, fwSize, shaExpected, signature);
                
                if (fota_success) {
                    Serial.println(F("[FOTA] Update successful - restarting in 2 seconds..."));
                    delay(2000);
                    ESP.restart();
                } else {
                    Serial.println(F("[FOTA] Update failed - continuing normal operation"));
                }
            }
            
            // STEP 4: After successful ACK from cloud → clear buffer
            Serial.println(F("[WORKFLOW] Successful ACK → clear buffer"));
                buffer_count = 0;
                buffer_write_index = 0;
                buffer_full = false;
                if (buffer != nullptr) {
                    memset(buffer, 0, sizeof(register_reading_t) * buffer_size);
                }
                
                // Reset retry counters on success
                upload_retry_count = 0;
                last_upload_attempt = 0;
                
                // WORKFLOW STEP 5: Buffer becomes free again for next cycle
                upload_in_progress = false;
                Serial.println(F("[WORKFLOW] Buffer free for next cycle"));
                
                reset_error_state();
        } else {
            // No response - upload failed
            Serial.println(F("[UPLOAD] Failed - no response from cloud"));
            upload_in_progress = false;  // Re-enable filling on upload failure
            upload_retry_count++;
            last_upload_attempt = current_time;
            Serial.print(F("[UPLOAD] Network failure - retry count: "));
            Serial.println(upload_retry_count);
        }
        
        memset(compressed_data, 0, compressed_data_len);
        compressed_data_len = 0;
        
    } else {
        log_error(ERROR_COMPRESSION_FAILED, "No compressed data available for upload");
        upload_in_progress = false;  // Re-enable filling on failure
        upload_retry_count++;
        last_upload_attempt = current_time;
        Serial.print(F("[UPLOAD] No data after compression - retry count: "));
        Serial.println(upload_retry_count);
        return;
    }
}

// Send immediate write command acknowledgment 
void send_write_command_ack(const String& status, const String& error_code, const String& error_message) {
    Serial.println(F("[COMMAND] Sending immediate write command acknowledgment"));
    
    // Create JSON payload for command result
    String json_payload;
    json_payload.reserve(256);
    json_payload = "{\"command_result\":{";
    json_payload += "\"status\":\"" + status + "\",";
    
    // Add current timestamp
    json_payload += "\"executed_at\":\"" + get_current_timestamp() + "\"";
    
    // Add error details if status is failed
    if (status == "failed" && error_code.length() > 0) {
        json_payload += ",\"error_code\":\"" + error_code + "\"";
        if (error_message.length() > 0) {
            json_payload += ",\"error_message\":\"" + error_message + "\"";
        }
    }
    
    json_payload += "}}";
    
    // Send to the command result endpoint
    String url = String(UPLOAD_API_BASE_URL) + "/api/cloud/command_result";
    String api_key = UPLOAD_API_KEY;
    String method = "POST";
    
    Serial.print(F("[COMMAND] Sending ACK: "));
    Serial.println(json_payload);
    
    // Send the acknowledgment (use JSON API function that returns response)
    String response = json_api_send_request(url, method, api_key, json_payload);
    
    if (response.length() > 0 && response.indexOf("success") >= 0) {
        Serial.println(F("[COMMAND] ✅ Write command ACK sent successfully"));
    } else {
        Serial.println(F("[COMMAND] ❌ Write command ACK failed"));
    }
}

void execute_command_task(void) {
    Serial.println(F("Executing command task..."));

    // FIXED: Always attempt to send result if available
    if (write_status.length() == 0) {
        Serial.println(F("[COMMAND] No result to report"));
        tasks[TASK_COMMAND_HANDLING].enabled = false;
        return;
    }

    String frame;
    frame.reserve(100);
    frame  = F("{\"command_result\":{");
    frame += F("\"status\":\"");
    frame += write_status;
    frame += F("\",\"executed_at\":\"");
    frame += write_executed_timestamp;
    frame += F("\"}}");

    frame = append_crc_to_frame(frame);
    
    String url;
    url.reserve(128);
    url = UPLOAD_API_BASE_URL;
    url += "/api/cloud/command_result";
    String api_key = UPLOAD_API_KEY;
    String method = "POST";
    
    api_command_request_with_retry(url, method, api_key, frame);

    write_status = "";
    write_executed_timestamp = "";
    tasks[TASK_COMMAND_HANDLING].enabled = false;
}

// FOTA task removed - now integrated into upload response handling
// The cloud sends FOTA manifest in the upload acknowledgment response
// See execute_upload_task() for FOTA integration

// Compress the buffer and add header
bool attempt_compression(register_reading_t* buffer, size_t* buffer_count) {
    int retry_count = 0;
    while (retry_count < MAX_COMPRESSION_RETRIES) {
        compression_metrics = compress_raw(buffer, *buffer_count, compressed_data);
        compressed_data_len = compression_metrics.compressed_payload_size;
        Serial.print(F("[COMPRESSION] Time: "));
        Serial.print(compression_metrics.cpu_time_us);
        Serial.println(F(" us"));

        if (compressed_data_len >= 5) {
            Serial.println(F("[COMPRESSION] Raw buffer compressed successfully"));
            return true;
        } else {
            retry_count++;
            Serial.print(F("[COMPRESSION] Failed. Retry "));
            Serial.print(retry_count);
            Serial.print(F("/"));
            Serial.println(MAX_COMPRESSION_RETRIES);
        }
    }

    log_error(ERROR_COMPRESSION_FAILED, "Compression failed after retries");
    return false;
}

void init_tasks_last_run(unsigned long start_time) {
    for (int i = 0; i < TASK_COUNT; i++) {
        tasks[i].last_run_ms = start_time;
    }
}

size_t aggregate_buffer_avg(const register_reading_t* buffer, size_t count, register_reading_t** out_buffer) {
    size_t agg_count = (count + AGG_WINDOW - 1) / AGG_WINDOW;

    register_reading_t* agg_buffer = (register_reading_t*)malloc(agg_count * sizeof(register_reading_t));
    if (!agg_buffer) return 0;

    size_t agg_idx = 0;
    for (size_t i = 0; i < count; i += AGG_WINDOW) {
        for (size_t reg = 0; reg < READ_REGISTER_COUNT; reg++) {
            uint32_t sum = 0;
            size_t actual = 0;

            for (size_t j = i; j < i + AGG_WINDOW && j < count; j++) {
                sum += buffer[j].values[reg];
                actual++;
            }

            uint16_t avg_val = (uint16_t)(sum / actual);
            agg_buffer[agg_idx].values[reg] = avg_val;
        }
        agg_idx++;
    }

    *out_buffer = agg_buffer;
    return agg_count;
}

// Unified command finalization
void finalize_command(const String& status) {
    write_status = status;
    write_executed_timestamp = get_current_timestamp();
    current_command.pending = false;
    tasks[TASK_WRITE_REGISTER].enabled = false;
    tasks[TASK_COMMAND_HANDLING].enabled = true;  // Enable result reporting
    
    Serial.print(F("[COMMAND] Finalized with status: "));
    Serial.println(status);
    
    // Send immediate acknowledgment for write commands
    if (status.startsWith("Success")) {
        send_write_command_ack("success");
    } else if (status.startsWith("Failed")) {
        String error_code = "MODBUS_ERROR";
        String error_message = status;
        
        // Map specific error types
        if (status.indexOf("Invalid value") >= 0) {
            error_code = "INVALID_VALUE";
        } else if (status.indexOf("Exception") >= 0) {
            error_code = "MODBUS_EXCEPTION";
        } else if (status.indexOf("No response") >= 0) {
            error_code = "TIMEOUT";
            error_message = "Modbus write timeout";
        } else if (status.indexOf("Invalid response") >= 0) {
            error_code = "INVALID_RESPONSE";
            error_message = "Invalid Modbus response";
        }
        
        send_write_command_ack("failed", error_code, error_message);
    }
}