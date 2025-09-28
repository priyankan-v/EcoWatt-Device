#include "api_client.h"
#include "config.h"
#include "error_handler.h"
#include "cloudAPI_handler.h"
#include <WiFi.h>
#include <HTTPClient.h>

bool api_init(void) {
    Serial.println(F("API client initialized"));
    return true;
}

String api_send_request(const String& url, const String& method, const String& api_key, const String& frame) {
    if (WiFi.status() != WL_CONNECTED) {
        log_error(ERROR_WIFI_DISCONNECTED, "WiFi not connected for API request");
        return "";
    }

    HTTPClient http;

    // Begin the HTTP request
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader(F("Content-Type"), F("application/json"));
    http.addHeader(F("Authorization"), api_key);

    // Pre-allocate request body
    String request_body;
    request_body.reserve(frame.length() + 20);
    request_body = F("{\"frame\":\"");
    request_body += frame;
    request_body += F("\"}");
    
    int http_code;
    if (method == "POST") {
        http_code = http.POST(request_body);
    } else if (method == "GET") {
        http_code = http.GET();
    } else {
        log_error(ERROR_INVALID_HTTP_METHOD, "Unsupported HTTP method");
        http.end();
        return "";
    }

    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();

        // Parse JSON response more robustly
        int start = response.indexOf(F("\"frame\":\""));
        if (start >= 0) {
            start += 9; // Length of "\"frame\":\""
            int end = response.indexOf('\"', start);

            if (end > start) {
                String frame_hex = response.substring(start, end);
                http.end();

                // Basic validation of hex string
                if (frame_hex.length() > 0 && frame_hex.length() % 2 == 0) {
                    return frame_hex;
                } else {
                    log_error(ERROR_INVALID_RESPONSE, "Invalid frame format in response");
                }
            }
        } else {
            log_error(ERROR_INVALID_RESPONSE, "Frame not found in response");
        }
    } else if (http_code > 0) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "HTTP error: %d", http_code);
        log_error(ERROR_HTTP_FAILED, error_msg);
    } else {
        log_error(ERROR_HTTP_TIMEOUT, "HTTP request timeout");
    }

    http.end();
    return "";
}

String upload_api_send_request(const String& url, const String& method, const String& api_key, const uint8_t* frame, size_t frame_length) {
    if (WiFi.status() != WL_CONNECTED) {
        log_error(ERROR_WIFI_DISCONNECTED, "WiFi not connected for API request");
        return "";
    }

    HTTPClient http;

    Serial.println(url);
    // Begin the HTTP request
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader(F("Content-Type"), F("application/octet-stream"));
    http.addHeader(F("Authorization"), api_key);
    
    int http_code;
    if (method == "POST") {
        http_code = http.POST((uint8_t*)frame, frame_length);
    } else {
        log_error(ERROR_INVALID_HTTP_METHOD, "Unsupported HTTP method");
        http.end();
        return "";
    }

    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();

        // Use cloud API response validation for upload endpoints
        if (validate_upload_response(response)) {
            http.end();
            return "success"; // Return success indicator
        } else {
            log_error(ERROR_INVALID_RESPONSE, "Upload response validation failed");
        }
    } else if (http_code > 0) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "HTTP error: %d", http_code);
        log_error(ERROR_HTTP_FAILED, error_msg);
    } else {
        log_error(ERROR_HTTP_TIMEOUT, "HTTP request timeout");
    }

    http.end();
    return "";
}

String api_send_request_with_retry(const String& url, const String& method, const String& api_key, const String& frame) {
    int retry_count = 0;
    error_code_t last_error_code = ERROR_NONE;

    while (retry_count <= MAX_RETRIES) {
        String response = api_send_request(url, method, api_key, frame);

        if (response.length() > 0) {
            // Success
            return response;
        }

        // Get the last error for retry decision
        if (WiFi.status() != WL_CONNECTED) {
            last_error_code = ERROR_WIFI_DISCONNECTED;
        } else {
            last_error_code = ERROR_HTTP_FAILED;
        }

        if (!should_retry(last_error_code, retry_count)) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Max retries exceeded for %s", url.c_str());
            log_error(ERROR_MAX_RETRIES_EXCEEDED, error_msg);
            break;
        }

        retry_count++;
        unsigned long delay_ms = get_retry_delay(retry_count - 1);

        Serial.print(F("Retrying API request in "));
        Serial.print(delay_ms);
        Serial.println(F(" ms..."));

        delay(delay_ms);

        // Try to reconnect WiFi if needed
        if (last_error_code == ERROR_WIFI_DISCONNECTED) {
            handle_wifi_reconnection();
        }
    }

    return "";
}

String upload_api_send_request_with_retry(const String& url, const String& method, const String& api_key, const uint8_t* frame, size_t frame_length) {
    int retry_count = 0;
    error_code_t last_error_code = ERROR_NONE;

    while (retry_count <= MAX_RETRIES) {
        String response = upload_api_send_request(url, method, api_key, frame, frame_length);

        if (response.length() > 0) {
            // Success
            return response;
        }

        // Get the last error for retry decision
        if (WiFi.status() != WL_CONNECTED) {
            last_error_code = ERROR_WIFI_DISCONNECTED;
        } else {
            last_error_code = ERROR_HTTP_FAILED;
        }

        if (!should_retry(last_error_code, retry_count)) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Max retries exceeded for %s", url.c_str());
            log_error(ERROR_MAX_RETRIES_EXCEEDED, error_msg);
            break;
        }

        retry_count++;
        unsigned long delay_ms = get_retry_delay(retry_count - 1);

        Serial.print(F("Retrying API request in "));
        Serial.print(delay_ms);
        Serial.println(F(" ms..."));

        delay(delay_ms);

        // Try to reconnect WiFi if needed
        if (last_error_code == ERROR_WIFI_DISCONNECTED) {
            handle_wifi_reconnection();
        }
    }

    return "";
}