#include "cloudAPI_handler.h"

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

String encrypt_upload_frame(const String& frame) {

    // mock function to simulate encryption
    return frame;
}

String generate_upload_frame_from_buffer(const String& frame) {
    if (frame.length() == 0) {
        Serial.println(F("Error: No data available in the buffer."));
        return "";
    }

    // mock function to simulate frame generation

    return frame;
}

String generate_upload_frame_from_buffer_with_encryption(const String& frame) {
    // Generate the JSON payload
    String generated_frame = generate_upload_frame_from_buffer(frame);

    if (generated_frame.length() == 0) {
        Serial.println(F("Error: Failed to generate upload frame."));
        return "";
    }

    // Encrypt the payload
    String encrypted_frame = encrypt_upload_frame(generated_frame);

    return encrypted_frame;
}

