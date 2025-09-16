#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include <time.h>

// Function to synchronize time with an NTP server
void syncTime();
time_t epochNow();

#endif // TIME_SYNC_H