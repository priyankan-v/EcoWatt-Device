#include "modbus_handler.h"
#include "config.h"
#include "calculateCRC.h"
#include "checkCRC.h"
#include "error_handler.h"

bool validate_modbus_response(const String& response) {
    // Check minimum length (slave_addr + function_code + data + CRC = minimum 6 chars)
    if (response.length() < 6) {
        log_error(ERROR_INVALID_RESPONSE, "Response too short");
        return false;
    }
    
    // Check if response length is even (hex pairs)
    if (response.length() % 2 != 0) {
        log_error(ERROR_INVALID_RESPONSE, "Invalid response length");
        return false;
    }
    
    // Verify CRC
    if (!checkCRC(response)) {
        log_error(ERROR_CRC_FAILED, "CRC validation failed");
        return false;
    }
    
    return true;
}

bool is_exception_response(const String& response) {
    if (response.length() < 4) {
        return false;
    }
    
    // Extract function code (second byte)
    String func_code_str = response.substring(2, 4);
    uint8_t func_code = strtoul(func_code_str.c_str(), nullptr, 16);
    
    // Exception responses have bit 7 set (0x80 | original_function_code)
    return (func_code & 0x80) != 0;
}

uint8_t get_exception_code(const String& response) {
    if (!is_exception_response(response) || response.length() < 6) {
        return 0;
    }
    
    // Exception code is in the third byte
    String exception_str = response.substring(4, 6);
    return strtoul(exception_str.c_str(), nullptr, 16);
}

bool is_valid_write_value(uint16_t register_addr, uint16_t value) {
    if (!(register_addr < MAX_REGISTERS)) {
        return false;
    }
    
    // Special validation for export power register
    if (register_addr == EXPORT_POWER_REGISTER) {
        return (value >= MIN_EXPORT_POWER && value <= MAX_EXPORT_POWER);
    }
    
    // General validation - allow full uint16_t range for other registers
    return true;
}

bool decode_response_registers(const String& response, uint16_t* values, size_t max_count, size_t* actual_count) {
    *actual_count = 0;
    
    if (!validate_modbus_response(response)) {
        return false;
    }
    
    if (is_exception_response(response)) {
        uint8_t exception_code = get_exception_code(response);
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Modbus exception: 0x%02X", exception_code);
        log_error(ERROR_MODBUS_EXCEPTION, error_msg);
        return false;
    }
    
    // Parse response: slave_addr(1) + func_code(1) + byte_count(1) + data(n) + crc(2)
    if (response.length() < 8) { // Minimum for valid data response
        log_error(ERROR_INVALID_RESPONSE, "Response too short for data");
        return false;
    }
    
    // Extract byte count
    String byte_count_str = response.substring(4, 6);
    uint8_t byte_count = strtoul(byte_count_str.c_str(), nullptr, 16);
    
    // Calculate number of registers (each register is 2 bytes)
    size_t register_count = byte_count / 2;
    
    if (register_count > max_count) {
        log_error(ERROR_INVALID_RESPONSE, "Too many registers in response");
        return false;
    }
    
    // Extract register values
    for (size_t i = 0; i < register_count; i++) {
        int start_pos = 6 + (i * 4); // Start after header, each register is 4 hex chars
        if (start_pos + 4 > response.length() - 4) { // -4 for CRC
            break;
        }
        
        String reg_str = response.substring(start_pos, start_pos + 4);
        values[i] = strtoul(reg_str.c_str(), nullptr, 16);
        (*actual_count)++;
    }
    
    return true;
}

String format_request_frame(uint8_t slave_addr, uint8_t function_code, uint16_t start_reg, uint16_t count_or_value) {
    char frame[17]; // 8 bytes = 16 hex chars + null terminator
    
    snprintf(frame, sizeof(frame), "%02X%02X%04X%04X", 
             slave_addr, function_code, start_reg, count_or_value);
    
    return String(frame);
}

String append_crc_to_frame(const String& frame_without_crc) {
    int frame_length = frame_without_crc.length() / 2;
    uint8_t frame_bytes[frame_length];
    
    // Convert hex string to bytes
    for (int i = 0; i < frame_length; i++) {
        String byte_str = frame_without_crc.substring(i * 2, i * 2 + 2);
        frame_bytes[i] = strtoul(byte_str.c_str(), nullptr, 16);
    }
    
    // Calculate CRC
    uint16_t crc = calculateCRC(frame_bytes, frame_length);
    
    // Append CRC (low byte first)
    char crc_hex[5];
    snprintf(crc_hex, sizeof(crc_hex), "%02X%02X", crc & 0xFF, (crc >> 8) & 0xFF);
    
    return frame_without_crc + String(crc_hex);
}

size_t get_expected_response_length(uint8_t function_code, uint16_t register_count) {
    switch (function_code) {
        case FUNCTION_CODE_READ:
            // slave_addr(1) + func_code(1) + byte_count(1) + data(register_count*2) + crc(2)
            return (5 + register_count * 2) * 2; // *2 for hex encoding
        case FUNCTION_CODE_WRITE:
            // slave_addr(1) + func_code(1) + register_addr(2) + value(2) + crc(2)
            return 8 * 2; // *2 for hex encoding
        default:
            return 0;
    }
}
