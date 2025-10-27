#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>
#include <time.h>

// --- CONFIGURATION ---
// Default to Sri Lanka time (UTC +5:30)
#define DEFAULT_GMT_OFFSET_SEC 19800
#define DEFAULT_DAYLIGHT_OFFSET_SEC 0
#define DEFAULT_NTP_SERVER "pool.ntp.org"

// Initializes NTP and stores last sync time
void init_time(const char* ntpServer = DEFAULT_NTP_SERVER,
               long gmtOffset_sec = DEFAULT_GMT_OFFSET_SEC,
               int daylightOffset_sec = DEFAULT_DAYLIGHT_OFFSET_SEC);

// Returns the current local timestamp (ISO 8601 format: YYYY-MM-DDTHH:MM:SS+05:30)
String get_current_timestamp();

#endif // TIME_UTILS_H
