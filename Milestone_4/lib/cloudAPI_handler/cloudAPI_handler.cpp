#include "cloudAPI_handler.h"
#include "calculateCRC.h"
#include "config.h"
#include "api_client.h"

bool validate_upload_response(const String& response) {
    if (response.length() == 0) {
        Serial.println(F("Error: Empty response received from the cloud API."));
        return false;
    }

    // Debug: Print the actual response
    Serial.print(F("[DEBUG] Cloud API Response: "));
    Serial.println(response);

    // Check for success status (flexible matching for whitespace variations)
    if (response.indexOf(F("\"status\"")) >= 0 && 
        (response.indexOf(F("\"success\"")) >= 0 || response.indexOf(F("success")) >= 0)) {
        Serial.println(F("Upload response validated successfully."));
        return true;
    }

    if (response.indexOf(F("\"error\"")) >= 0) {
        int msg_start = response.indexOf(F("\"error\":\""));
        if (msg_start >= 0) {
            int msg_end = response.indexOf(F("\""), msg_start + 12);
            if (msg_end > msg_start) {
                String error_message = response.substring(msg_start + 12, msg_end);
                Serial.print(F("Error: Upload failed with status: "));
                Serial.println(error_message);
            }
        }
        return false;
    }

    Serial.println(F("Error: Unrecognized response format from the cloud API."));
    return false;
}

bool parse_config_update_from_response(const String& response, String& config_update_json) {
    if (response.length() == 0) {
        return false;
    }

    // Parse JSON response to look for config_update section
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.print(F("[CONFIG] JSON parse error in response: "));
        Serial.println(error.c_str());
        return false;
    }

    // Check if config_update exists in the response
    if (doc["config_update"].is<JsonObject>()) {
        Serial.println(F("[CONFIG] Configuration update found in response"));
        
        // Create the config update JSON string
        JsonDocument config_doc;
        config_doc["config_update"] = doc["config_update"];
        
        serializeJson(config_doc, config_update_json);
        
        Serial.print(F("[CONFIG] Extracted config update: "));
        Serial.println(config_update_json);
        return true;
    }

    return false;
}

void send_config_ack_to_cloud(const String& ack_json) {
    if (ack_json.length() == 0) {
        Serial.println(F("[CONFIG] Empty ACK, skipping upload"));
        return;
    }

    Serial.print(F("[CONFIG] Sending ACK to cloud: "));
    Serial.println(ack_json);

    // Use the API client to send the acknowledgment
    String url = UPLOAD_API_BASE_URL;
    url += "/api/config_ack";
    String method = "POST";
    String api_key = UPLOAD_API_KEY;

    // Send the JSON request
    extern String json_api_send_request(const String& url, const String& method, const String& api_key, const String& json_body);
    String response = json_api_send_request(url, method, api_key, ack_json);
    
    if (response.length() > 0) {
        Serial.println(F("[CONFIG] ACK successfully sent to cloud"));
    } else {
        Serial.println(F("[CONFIG] Failed to send ACK to cloud"));
    }
}

void encrypt_compressed_frame(const uint8_t* data, size_t len, uint8_t* output_data) {
    // Placeholder encryption - AES-128-CBC would be implemented here
    // For production: implement AES encryption with proper key management
    
    Serial.print(F("[ENCRYPTION STUB] Processing "));
    Serial.print(len);
    Serial.println(F(" bytes"));
    
    // For now, just copy the data (no actual encryption)
    memcpy(output_data, data, len);
    
    // Placeholder: In production, this would:
    // 1. Generate or use stored AES key
    // 2. Generate random IV
    // 3. Encrypt data using AES-128-CBC
    // 4. Prepend IV to encrypted data
}

void calculate_and_add_mac(const uint8_t* data, size_t len, uint8_t* mac_output) {
    // Placeholder MAC calculation - real HMAC-SHA256 will be implemented later
    // For production:
    //  1. Load shared secret key
    //  2. Calculate HMAC-SHA256(key, data, len)
    //  3. Truncate result to required length (e.g., 8 or 16 bytes)

    // Stub: Generate deterministic but fake MAC for testing
    for (size_t i = 0; i < 8; i++) {
        mac_output[i] = (uint8_t)(0xAA ^ (i * 0x11));
    }

    // Debug logs
    Serial.print(F("[SECURITY] MAC stub calculated for "));
    Serial.print(len);
    Serial.println(F(" bytes"));

    Serial.print(F("[SECURITY] MAC: "));
    for (size_t i = 0; i < 8; i++) {
        Serial.print(mac_output[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}


void append_crc_to_upload_frame(const uint8_t* encrypted_frame, size_t frame_length, uint8_t* output_frame) {
    memcpy(output_frame, encrypted_frame, frame_length);
        
    // Calculate CRC (in production, CRC would cover encrypted_data + MAC)
    uint16_t crc = calculateCRC(encrypted_frame, frame_length);
    
    // Append CRC to the end of the frame (little-endian format)
    output_frame[frame_length] = crc & 0xFF;         // Low byte first
    output_frame[frame_length + 1] = (crc >> 8) & 0xFF; // High byte second
    
    // Note: In production, frame structure would be:
    // [encrypted_data][MAC][CRC] where CRC covers encrypted_data+MAC
    
    return;
}
