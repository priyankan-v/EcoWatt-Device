#ifndef CLOUD_API_HANDLER_H
#define CLOUD_API_HANDLER_H

#include <Arduino.h>

bool validate_upload_response(const String& response);
void encrypt_compressed_frame(const uint8_t* data, size_t len, uint8_t* output_data);
void append_crc_to_upload_frame(const uint8_t* encrypted_frame, size_t frame_length, uint8_t* output_frame);

#endif