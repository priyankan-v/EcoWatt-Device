#include "time_sync.h"
#include <WiFi.h>
#include <time.h>

time_t epochAtSync = 0;     // Stores epoch when synced
uint32_t millisAtSync = 0;  // Stores millis() when synced
unsigned long lastSyncAttempt = 0; // Track when last sync was tried
const unsigned long syncRetryInterval = 60000; // Retry every 60s if failed

void syncTime() {
    const char* ntpServer = "pool.ntp.org";
    const long gmtOffset_sec = 19800; // IST = UTC+5:30
    const int daylightOffset_sec = 0;

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeInfo;
    int retry = 0;
    // Wait up to ~5 seconds for sync
    while (!getLocalTime(&timeInfo) && retry < 10) {
        delay(500);
        retry++;
    }

    if (retry < 10) {
        time(&epochAtSync);
        millisAtSync = millis();
        Serial.print(F("Time synced! Local epoch(IST): "));
        Serial.println(epochAtSync);
    } else {
        Serial.println(F("Failed to synchronize time with NTP server."));
        epochAtSync = 0; // mark invalid
    }
    lastSyncAttempt = millis(); // record sync attempt time
}

// Returns current local epoch like millis()
unsigned long epochNow() {
    if (epochAtSync == 0) {
        // Retry if enough time passed since last attempt
        if (millis() - lastSyncAttempt >= syncRetryInterval) {
            syncTime();
        }
        return 0; // Still invalid until successful
    }
    return (unsigned long)(epochAtSync + (millis() - millisAtSync) / 1000UL);
}

