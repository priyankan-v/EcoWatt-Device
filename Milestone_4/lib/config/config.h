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
#define UPLOAD_API_BASE_URL "https://eco-watt-cloud.vercel.app"
#define UPLOAD_API_KEY "ColdPlay2025"
// #define MANIFEST_API_BASE_URL "https://eco-watt-cloud.vercel.app/api/fota/manifest"
// #define LOG_BASE_URL "https://eco-watt-cloud.vercel.app/api/fota/log"
#define NTP_SERVER "pool.ntp.org"
#define UPLOAD_PSK "ColdPlay@EcoWatt2025"
#define NONCE_ADDRESS 0  // EEPROM address for storing nonce

// HTTP configuration
#define HTTP_TIMEOUT_MS 10000
#define MAX_RETRIES 3
#define RETRY_BASE_DELAY_MS 1000UL
#define MAX_RETRY_DELAY_MS 8000UL

// Timing configuration
#define POLL_INTERVAL_MS 3000
#define WRITE_INTERVAL_MS  (UPLOAD_INTERVAL_MS / 2) 
#define HEALTH_CHECK_INTERVAL_MS 30000
#define WATCHDOG_TIMEOUT_S 30
#define UPLOAD_INTERVAL_MS 15000 //900000; // 15 minutes
#define FOTA_INTERVAL_MS 15000 // Change this later on
#define COMMAND_INTERVAL_MS 15000

// Modbus configuration
#define SLAVE_ADDRESS 0x11
#define FUNCTION_CODE_READ 0x03
#define FUNCTION_CODE_WRITE 0x06
#define MAX_REGISTERS 10
#define EXPORT_POWER_REGISTER 8
#define MIN_EXPORT_POWER 0
#define MAX_EXPORT_POWER 100

// Read registers array
#define READ_REGISTER_COUNT 10 
extern const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT];

// System configuration
#define SERIAL_BAUD_RATE 115200
#define MEMORY_BUFFER_SIZE (UPLOAD_INTERVAL_MS / POLL_INTERVAL_MS)

// Compression configuration
#define MAX_COMPRESSION_SIZE (MEMORY_BUFFER_SIZE * 2 * READ_REGISTER_COUNT + 5)
#define MAX_COMPRESSION_RETRIES 3 // Maximum number of compression retries
#define MAX_PAYLOAD_SIZE 200 // Maximum allowed payload size before using aggregation
#define AGG_WINDOW 10 // Samples per aggregation window

// Buffer behavior configuration
#define BUFFER_FULL_BEHAVIOR_CIRCULAR 1  // Option A: Overwrite oldest data (circular buffer)
#define BUFFER_FULL_BEHAVIOR_STOP 0     // Option B: Stop new acquisitions until space is free
#define BUFFER_FULL_BEHAVIOR BUFFER_FULL_BEHAVIOR_STOP  // Choose behavior when buffer is full

// Register gains (stored in PROGMEM)
extern const PROGMEM float REGISTER_GAINS[MAX_REGISTERS];
extern const PROGMEM char* REGISTER_UNITS[MAX_REGISTERS];


#endif