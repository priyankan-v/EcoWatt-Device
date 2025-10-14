#include "time_utils.h"

static unsigned long lastSyncMillis = 0;   // Track last NTP sync
static bool timeInitialized = false;       // Track if NTP has synced at least once

void init_time(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec) {
    Serial.printf("[TimeUtils] Syncing NTP from %s...\n", ntpServer);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Wait for valid time (non-blocking check with timeout)
    time_t now;
    int retry = 0;
    const int maxRetries = 5;
    do {
        delay(500);
        time(&now);
        retry++;
    } while (now < 100000 && retry < maxRetries);  // Check if time is valid

    if (now < 100000) {
        Serial.println("[TimeUtils] Failed to sync NTP");
        timeInitialized = false;
    } else {
        Serial.println("[TimeUtils] NTP time synced successfully");
        timeInitialized = true;
        lastSyncMillis = millis();
    }
}

String get_current_timestamp() {
    time_t now;
    struct tm timeinfo;
    char buf[30];

    time(&now);

    // Fallback message if NTP not yet synced
    if (now < 100000) {
        return "1970-01-01T00:00:00IST";
    }

    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SIST", &timeinfo);
    return String(buf);
}
