#include "scheduler.h"
#include "config.h"
#include "api_client.h"
#include "modbus_handler.h"
#include "error_handler.h"
#include "cloudAPI_handler.h"
#include "compressor.h"
#include "fota.h"
#include "encryptionAndSecurity.h"
#include "command_parse.h"
#include "time_utils.h"

extern NonceManager nonceManager; // Declare the global instance from main.cpp

// Task definitions
static scheduler_task_t tasks[TASK_COUNT] = {
    {TASK_READ_REGISTERS, POLL_INTERVAL_MS, 0, true},
    {TASK_WRITE_REGISTER, WRITE_INTERVAL_MS, 0, false},
    {TASK_UPLOAD_DATA, UPLOAD_INTERVAL_MS, 0, true},
    {TASK_PERFORM_FOTA, FOTA_INTERVAL_MS, 0, true},
    {TASK_COMMAND_HANDLING, COMMAND_INTERVAL_MS, 0, false}
};

// Single buffer definition - Buffer Rules Implementation
static register_reading_t buffer[MEMORY_BUFFER_SIZE];
static size_t buffer_count = 0;
static size_t buffer_write_index = 0;  // For circular buffer behavior
static bool upload_in_progress = false;  // Prevents filling during upload
static bool buffer_full = false;  // Tracks if buffer is full
static unsigned long last_upload_attempt = 0;  // For retry delays
static int upload_retry_count = 0;  // Track retry attempts

// Write command tracking
static command_state_t current_command = {false, 0, 0};
static String write_status = ""; // Track if last write was successful
static String write_executed_timestamp = ""; // Timestamp of last write execution

uint8_t compressed_data[MAX_COMPRESSION_SIZE] = {0}; // Output buffer for compression
size_t compressed_data_len = 0; // Length of compressed data
compression_metrics_t compression_metrics = {0}; // Metrics of last compression

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
                case TASK_COMMAND_HANDLING:
                    execute_command_task();
                    break;
                case TASK_UPLOAD_DATA:
                    execute_upload_task();
                    break;
                case TASK_PERFORM_FOTA:
                    execute_fota_task();
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
    buffer_write_index = (buffer_write_index + 1) % MEMORY_BUFFER_SIZE;

    // Track buffer usage
    if (!buffer_full) {
        buffer_count++;
        if (buffer_count >= MEMORY_BUFFER_SIZE) {
            buffer_full = true;
            #if BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_CIRCULAR
                Serial.println(F("[BUFFER] Buffer full - using circular overwrite"));
            #elif BUFFER_FULL_BEHAVIOR == BUFFER_FULL_BEHAVIOR_STOP
                Serial.println(F("[BUFFER] Buffer full - will stop acquisitions"));
            #endif
        }
    }

    if (buffer_count % MEMORY_BUFFER_SIZE == 0 || buffer_full) {  // Log every (MEMORY_BUFFER_SIZE) samples or when full
        Serial.print(F("[BUFFER] Samples: "));
        Serial.print(buffer_count);
        Serial.print(F("/"));
        Serial.print(MEMORY_BUFFER_SIZE);
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
    // Serial.println(F("Executing read task..."));
    
    // Generate read frame
    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_READ, pgm_read_word(&READ_REGISTERS[0]), READ_REGISTER_COUNT);
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
        /*
        // Encrypt the frame
        uint8_t encrypted_frame_len = 16 + ((compressed_data_len + 15) / 16) * 16; // AES block size alignment
        uint8_t encrypted_frame[encrypted_frame_len];
        // memset(encrypted_frame, 0, encrypted_frame_len);
        encrypt_compressed_frame(compressed_data_frame, compressed_data_len + 1, encrypted_frame); 

        // Add MAC 
        uint8_t encrypted_frame_with_mac[encrypted_frame_len + 8];
        memcpy(encrypted_frame_with_mac, encrypted_frame, encrypted_frame_len);
        calculate_and_add_mac(encrypted_frame, encrypted_frame_len, encrypted_frame_with_mac + encrypted_frame_len);
        */
        
        // Add CRC for entire frame
        uint8_t upload_frame_with_crc[compressed_data_len + 3]; // metadata + data + CRC
        append_crc_to_upload_frame(compressed_data_frame, compressed_data_len + 1, upload_frame_with_crc);
        
        Serial.print(F("[UPLOAD] Final frame:"));
        Serial.print(F("data("));
        Serial.print(compressed_data_len + 1);
        Serial.print(F(") + crc(2) = "));
        Serial.print(compressed_data_len + 3);
        Serial.println(F(" bytes total"));

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

        // Encode payload to Base64 directly from the byte array
        String upload_frame_base64 = encodeBase64(upload_frame_with_crc, compressed_data_len + 3);
        Serial.print(F("[SECURITY] Base64 payload length (size_t): "));
        Serial.println(upload_frame_base64.length());

        // Generate MAC for the upload frame
        String mac = generateMAC(upload_frame_base64);
        Serial.print(F("[SECURITY] Generated MAC: "));
        Serial.println(mac);

        String response = upload_api_send_request_with_retry(url, method, api_key, upload_frame_with_crc, compressed_data_len + 3, String(nonce), mac);

        if (response.length() > 0) {
            String action;
            uint16_t reg = 0;
            uint16_t val = 0;

            if (extract_command(response, action, reg, val)) {
                Serial.println(F("[COMMAND] Command detected in cloud response"));
                
                if (action.equalsIgnoreCase("write_register")) {
                    Serial.println(F("[COMMAND] Validating WRITE command"));

                    // Store command atomically
                    current_command.pending = true;
                    current_command.register_address = reg;
                    current_command.value = val;
                    
                    tasks[TASK_WRITE_REGISTER].enabled = true;
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
            
            // WORKFLOW STEP 4: After successful ACK from cloud → clear buffer
            Serial.println(F("[WORKFLOW] Successful ACK → clear buffer"));
                buffer_count = 0;
                buffer_write_index = 0;
                buffer_full = false;
                memset(buffer, 0, sizeof(register_reading_t) * MEMORY_BUFFER_SIZE);
                
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

void execute_fota_task() {
    perform_FOTA_with_logging();
}

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
}