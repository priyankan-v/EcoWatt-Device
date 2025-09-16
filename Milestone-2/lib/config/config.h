#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <pgmspace.h>

// WiFi credentials
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_DELAY_MS 5000

// API configuration
#define API_BASE_URL "http://20.15.114.131:8080"
#define API_KEY "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YWQzOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWFjOQ=="

// Cloud Upload API Configuration
#define UPLOAD_API_BASE_URL "http://127.0.0.1:8080/"
#define UPLOAD_API_KEY "ColdPlay2025"

// HTTP configuration
#define HTTP_TIMEOUT_MS 10000
#define MAX_RETRIES 3
#define RETRY_BASE_DELAY_MS 1000UL
#define MAX_RETRY_DELAY_MS 8000UL

// Timing configuration
#define POLL_INTERVAL_MS 5000
#define WRITE_INTERVAL_MS 25000
#define HEALTH_CHECK_INTERVAL_MS 30000
#define WATCHDOG_TIMEOUT_S 30
#define UPLOAD_INTERVAL_MS 15000 //900000; // 15 minutes

// Modbus configuration
#define SLAVE_ADDRESS 0x11
#define FUNCTION_CODE_READ 0x03
#define FUNCTION_CODE_WRITE 0x06
#define MAX_REGISTERS 10
#define EXPORT_POWER_REGISTER 8
#define MIN_EXPORT_POWER 0
#define MAX_EXPORT_POWER 100

// System configuration
#define SERIAL_BAUD_RATE 115200
#define MEMORY_BUFFER_SIZE (UPLOAD_INTERVAL_MS / POLL_INTERVAL_MS)
#define MAX_COMPRESSION_SIZE (MEMORY_BUFFER_SIZE * (4 + 2*READ_REGISTER_COUNT))

// Register gains (stored in PROGMEM)
extern const PROGMEM float REGISTER_GAINS[MAX_REGISTERS];
extern const PROGMEM char* REGISTER_UNITS[MAX_REGISTERS];

// Read registers array
#define READ_REGISTER_COUNT 10 
extern const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT];

#endif