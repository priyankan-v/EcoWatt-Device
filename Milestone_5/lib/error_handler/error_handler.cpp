#include "error_handler.h"
#include "config.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Error state tracking
static error_code_t last_error = ERROR_NONE;
static unsigned long last_error_time = 0;
static int consecutive_errors = 0;

// System health tracking
static unsigned long last_health_check = 0;
static bool watchdog_enabled = false;

void error_handler_init(void) {
    // Initialize watchdog
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
    watchdog_enabled = true;
    
    Serial.println(F("Error handler initialized"));
}

void log_error(error_code_t error_code, const char* message) {
    last_error = error_code;
    last_error_time = millis();
    consecutive_errors++;
    
    Serial.print(F("ERROR ["));
    Serial.print(error_code);
    Serial.print(F("]: "));
    Serial.println(message);
}

bool should_retry(error_code_t error_code, int retry_count) {
    if (retry_count >= MAX_RETRIES) {
        return false;
    }
    
    switch (error_code) {
        case ERROR_WIFI_DISCONNECTED:
        case ERROR_HTTP_TIMEOUT:
        case ERROR_HTTP_FAILED:
            return true;
        case ERROR_INVALID_RESPONSE:
        case ERROR_CRC_FAILED:
            return retry_count < 2; // Fewer retries for data integrity issues
        case ERROR_MODBUS_EXCEPTION:
        case ERROR_INVALID_REGISTER:
            return false; // Don't retry protocol errors
        case ERROR_INVALID_HTTP_METHOD:
        default:
            return false;
    }
}

unsigned long get_retry_delay(int retry_count) {
    // Exponential backoff with jitter
    unsigned long base_delay = RETRY_BASE_DELAY_MS * (1 << retry_count);
    unsigned long jitter = random(0, base_delay / 4);
    unsigned long delay = base_delay + jitter;
    
    return min(delay, MAX_RETRY_DELAY_MS);
}

void reset_error_state(void) {
    last_error = ERROR_NONE;
    consecutive_errors = 0;
}

bool is_critical_error(error_code_t error_code) {
    return (error_code == ERROR_MAX_RETRIES_EXCEEDED || 
            consecutive_errors > MAX_RETRIES * 2);
}

void check_system_health(void) {
    unsigned long current_time = millis();
    
    if (current_time - last_health_check < HEALTH_CHECK_INTERVAL_MS) {
        return;
    }
    
    last_health_check = current_time;
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        log_error(ERROR_WIFI_DISCONNECTED, "WiFi disconnected during health check");
        handle_wifi_reconnection();
    }
    
    // Check error frequency
    if (consecutive_errors > MAX_RETRIES) {
        Serial.println(F("WARNING: High error frequency detected"));
    }
    
    // Reset error counter periodically
    if (current_time - last_error_time > 300000) { // 5 minutes
        consecutive_errors = 0;
    }
}

bool handle_wifi_reconnection(void) {
    Serial.println(F("Attempting WiFi reconnection..."));
    
    WiFi.disconnect();
    delay(1000);
    
    return wifi_init();
}

void feed_watchdog(void) {
    if (watchdog_enabled) {
        esp_task_wdt_reset();
    }
}
