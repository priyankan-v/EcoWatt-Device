#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>

// Initialize the API client
bool api_init(void);

// Send an API request
String api_send_request(const String& url, const String& method, const String& api_key, const String& frame);

// Send an API request with retry logic
String api_send_request_with_retry(const String& url, const String& method, const String& api_key, const String& frame);

#endif