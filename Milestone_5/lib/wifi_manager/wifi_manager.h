#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "time_utils.h"

// Function prototypes
bool wifi_init(void);
bool wifi_is_connected(void);

#endif