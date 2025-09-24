#include "cloudAPI_handler.h"
#include "calculateCRC.h"

bool validate_upload_response(const String& response) {
    if (response.length() == 0) {
        Serial.println(F("Error: Empty response received from the cloud API."));
        return false;
    }

    if (response.indexOf(F("\"status\":\"success\"")) >= 0) {
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

void calculate_mac(const uint8_t* data, size_t len, uint8_t* mac_output) {
    // Placeholder MAC calculation - HMAC-SHA256 would be implemented here
    // For production: implement HMAC-SHA256 with shared secret key
    
    Serial.print(F("[MAC STUB] Calculating MAC for "));
    Serial.print(len);
    Serial.println(F(" bytes"));
    
    // Placeholder MAC (would be 32 bytes for HMAC-SHA256)
    // For now, just fill with pattern for demonstration
    for (size_t i = 0; i < 8; i++) { // 8-byte placeholder MAC
        mac_output[i] = (uint8_t)(0xAA ^ (i * 0x11));
    }
    
    // Placeholder: In production, this would:
    // 1. Use stored HMAC key
    // 2. Calculate HMAC-SHA256(key, data)
    // 3. Return first 8-16 bytes of HMAC
}

void append_crc_to_upload_frame(const uint8_t* encrypted_frame, size_t frame_length, uint8_t* output_frame) {
    memcpy(output_frame, encrypted_frame, frame_length);
    
    // Placeholder: Calculate MAC (in production, this would be appended before CRC)
    uint8_t mac[8]; // Placeholder MAC size
    calculate_mac(encrypted_frame, frame_length, mac);
    
    Serial.print(F("[SECURITY] MAC calculated: "));
    for (size_t i = 0; i < 8; i++) {
        Serial.print(mac[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Calculate CRC (in production, CRC would cover encrypted_data + MAC)
    uint16_t crc = calculateCRC(encrypted_frame, frame_length);
    
    // Append CRC to the end of the frame (little-endian format)
    output_frame[frame_length] = crc & 0xFF;         // Low byte first
    output_frame[frame_length + 1] = (crc >> 8) & 0xFF; // High byte second
    
    // Note: In production, frame structure would be:
    // [encrypted_data][MAC][CRC] where CRC covers encrypted_data+MAC
    
    return;
}
