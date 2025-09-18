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
    // Simple XOR encryption with a fixed key for demonstration purposes
    return;
}

void append_crc_to_upload_frame(const uint8_t* encrypted_frame, size_t frame_length, uint8_t* output_frame) {

    memcpy(output_frame, encrypted_frame, frame_length);
    // Calculate CRC
    uint16_t crc = calculateCRC(encrypted_frame, frame_length);
    // Append CRC to the end of the frame
    output_frame[frame_length] = crc & 0xFF;         // Low byte    
    output_frame[frame_length + 1] = (crc >> 8) & 0xFF; // High byte

    return;
}
