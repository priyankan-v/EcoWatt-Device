#ifndef CLOUD_API_HANDLER_H
#define CLOUD_API_HANDLER_H

#include <Arduino.h>

bool validate_upload_response(const String& response);
String encrypt_upload_frame(const String& frame);
String generate_upload_frame_from_buffer(const String& frame);
String generate_upload_frame_from_buffer_with_encryption(const String& frame);

#endif