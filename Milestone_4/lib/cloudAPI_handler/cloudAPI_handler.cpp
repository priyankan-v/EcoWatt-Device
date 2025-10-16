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

bool parse_fota_manifest_from_response(const String& response, 
                                       int& job_id, 
                                       String& fwUrl, 
                                       size_t& fwSize, 
                                       String& shaExpected, 
                                       String& signature) {
    if (response.length() == 0) {
        return false;
    }

    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        return false;
    }

    // Check if fota section exists
    if (!doc["fota"].is<JsonObject>()) {
        return false;
    }

    JsonObject fota = doc["fota"];
    
    // Extract FOTA parameters
    if (fota.containsKey("job_id") && 
        fota.containsKey("fwUrl") && 
        fota.containsKey("fwSize") && 
        fota.containsKey("shaExpected") && 
        fota.containsKey("signature")) {
        
        job_id = fota["job_id"];
        fwUrl = fota["fwUrl"].as<String>();
        fwSize = fota["fwSize"];
        shaExpected = fota["shaExpected"].as<String>();
        signature = fota["signature"].as<String>();
        
        Serial.println(F("[FOTA] Manifest parsed from cloud response"));
        return true;
    }

    return false;
}

void encrypt_compressed_frame(const uint8_t* data, size_t len, uint8_t* output_data) {
    // Deprecated - replaced with real AES-256-CBC encryption
    // This function is no longer used
    Serial.println(F("[WARNING] encrypt_compressed_frame() is deprecated"));
    memcpy(output_data, data, len);
}

void calculate_and_add_mac(const uint8_t* data, size_t len, uint8_t* mac_output) {
    // Deprecated - MAC is now calculated using generateMAC() from encryptionAndSecurity
    // This function is no longer used
    Serial.println(F("[WARNING] calculate_and_add_mac() is deprecated"));
    memset(mac_output, 0, 8);
}

void append_crc_to_upload_frame(const uint8_t* frame, size_t frame_length, uint8_t* output_frame) {
    // Copy original frame
    memcpy(output_frame, frame, frame_length);
    
    // Calculate CRC for the frame
    uint16_t crc = calculateCRC(frame, frame_length);
    
    // Append CRC (little-endian)
    output_frame[frame_length] = crc & 0xFF;           // Low byte
    output_frame[frame_length + 1] = (crc >> 8) & 0xFF; // High byte
    
    Serial.printf("[CRC] Calculated CRC: 0x%04X for %d bytes\n", crc, frame_length);
}
