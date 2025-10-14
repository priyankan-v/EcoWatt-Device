#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>

// Initialize the API client
bool api_init(void);

// Send an API request
String api_send_request(const String& url, const String& method, const String& api_key, const String& frame);

// Send an API request with retry logic
String api_send_request_with_retry(const String& url, const String& method, const String& api_key, const String& frame);

// Send an API request for uploading data
String upload_api_send_request(const String& url, const String& method, const String& api_key, const uint8_t* frame, size_t frame_length);

// Send an upload API request with retry logic (simple version)
String upload_api_send_request_with_retry(const String& url, const String& method, const String& api_key, const uint8_t* frame, size_t frame_length);

// Send an upload API request with retry logic (with nonce and MAC)
String upload_api_send_request_with_retry(const String& url, const String& method, const String& api_key, const uint8_t* frame, size_t frame_length, const String& nonce, const String& mac);

// Send a JSON API request (for config acknowledgments)
String json_api_send_request(const String& url, const String& method, const String& api_key, const String& json_body);

#endif