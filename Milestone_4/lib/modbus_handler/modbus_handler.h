#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

#include <Arduino.h>

// Modbus response validation
bool validate_modbus_response(const String& response);
bool is_exception_response(const String& response);
uint8_t get_exception_code(const String& response);

// Register validation
bool is_valid_write_value(uint16_t register_addr, uint16_t value);

// Response processing
bool decode_response_registers(const String& response, uint16_t* values, size_t max_count, size_t* actual_count);
String format_request_frame(uint8_t slave_addr, uint8_t function_code, uint16_t start_reg, uint16_t count_or_value);

// Frame utilities
String append_crc_to_frame(const String& frame_without_crc);
size_t get_expected_response_length(uint8_t function_code, uint16_t register_count);

#endif
