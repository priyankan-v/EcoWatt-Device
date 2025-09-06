#include "api_client.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

bool api_init() {
    // Dummy implementation for API initialization
    return true; // Return true to indicate success
}

String api_send_request(const String& endpoint, const String& frame) {
    
    HTTPClient http;
    String url = String(API_BASE_URL) + "/api/inverter/" + endpoint;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", API_KEY);
    
    String request_body = "{\"frame\":\"" + frame + "\"}";
    
    int http_code = http.POST(request_body);
    
    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();
        
        int start = response.indexOf("\"frame\":\"") + 9;
        int end = response.indexOf("\"", start);
        
        if (start >= 9 && end > start) {
            String frame_hex = response.substring(start, end);
            http.end();
            return frame_hex;
        }
    } else {
        Serial.print("HTTP error: ");
        Serial.println(http_code);
    }
    
    http.end();
    return "";
}