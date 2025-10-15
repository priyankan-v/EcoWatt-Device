#include "wifi_manager.h"
#include "config.h"

bool wifi_init(void) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        init_time();
        return true;
    } else {
        Serial.println("\nWiFi connection failed");
        return false;
    }
}

bool wifi_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}