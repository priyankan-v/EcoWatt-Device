#include <Arduino.h>
#include <config.h>
#include <wifi_manager.h>
#include <api_client.h>
#include <error_handler.h>
#include <scheduler.h>
#include <modbus_handler.h>
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println(F("EcoWatt Device - Milestone 4"));
    
    // Initialize modules
    error_handler_init();
    
    // Initialize WiFi
    if (!wifi_init()) {
        log_error(ERROR_WIFI_DISCONNECTED, "Failed to initialize WiFi");
        Serial.println(F("Failed to initialize WiFi. Restarting in 5 seconds..."));
        delay(WIFI_RETRY_DELAY_MS);
        ESP.restart();
    }
    
    // Initialize API client
    if (!api_init()) {
        log_error(ERROR_HTTP_FAILED, "Failed to initialize API client");
        Serial.println(F("API client initialization failed"));
    } else {
        Serial.println(F("System initialized successfully"));
    }
    
    Serial.println(F("Starting main operation loop..."));
    Serial.println();

    unsigned long now = millis();
    init_tasks_last_run(now);
}

void loop() {
    // Run the scheduler (handles all periodic tasks)
    scheduler_run();
    
    // Small delay to prevent tight looping
    delay(100);
}
