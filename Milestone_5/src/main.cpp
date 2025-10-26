#include <Arduino.h>
#include <config.h>
#include <config_manager.h>
#include <wifi_manager.h>
#include <api_client.h>
#include <error_handler.h>
#include <scheduler.h>
#include <modbus_handler.h>
#include <encryptionAndSecurity.h>
#include "sdkconfig.h"
#include "esp_pm.h"
#include "driver/uart.h"

#if !CONFIG_AUTOSTART_ARDUINO

extern "C" void app_main()
{
    // initialize arduino library before we start the tasks
    initArduino();
}
#else

NonceManager nonceManager;

esp_pm_config_esp32_t pm_config;  // Power management configuration
bool pm_applied = false;

void applyPMConfig(int minFreq, int maxFreq, bool enableLightSleep = false) {
    pm_config.max_freq_mhz = maxFreq;
    pm_config.min_freq_mhz = minFreq;
    pm_config.light_sleep_enable = enableLightSleep;

    esp_err_t err = esp_pm_configure(&pm_config);
    if (err == ESP_OK) {
        Serial.printf("Frequency set: %d MHz (min), %d MHz (max)\n", minFreq, maxFreq);
        pm_applied = true;
    } else {
        Serial.printf("esp_pm_configure failed (code %d)\n", err);
        pm_applied = false;
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println(F("EcoWatt Device - Milestone 5"));
    
    // Initialize modules
    error_handler_init();
    nonceManager.begin();
    
    // Initialize WiFi
    if (!wifi_init()) {
        log_error(ERROR_WIFI_DISCONNECTED, "Failed to initialize WiFi");
        Serial.println(F("Failed to initialize WiFi. Restarting in 5 seconds..."));
        delay(WIFI_RETRY_DELAY_MS);
        ESP.restart();
    }
    
    // Initialize ConfigManager
    if (!config_manager_init()) {
        Serial.println(F("Failed to initialize ConfigManager"));
        // Continue with default configuration
    } else {
        Serial.println(F("ConfigManager initialized successfully"));
    }
    
    // Configuration is now handled through cloud upload responses
    Serial.println(F("Configuration updates integrated with cloud communication"));
    
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

    if (POWER_MANAGMENT && DVFS && !pm_applied) {
        Serial.print("Initial CPU Frequency:");
        Serial.print(getCpuFrequencyMhz());
        Serial.println(" MHz");

        Serial.print("Initial APB Frequency:");
        Serial.print(getApbFrequency());
        Serial.println(" Hz");
        applyPMConfig(80, 160, false);
    };

    if (POWER_MANAGMENT && SERIAL_GATING) {
        uart_driver_delete(UART_NUM_1);
        uart_driver_delete(UART_NUM_2);
    };
}

void loop() {
    // Run the scheduler (handles all periodic tasks)
    scheduler_run();
    
    // Process HTTP configuration requests
    // Configuration processing is now integrated with cloud upload responses
    
    // Small delay to prevent tight looping
    delay(100);
}
#endif