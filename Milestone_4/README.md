# EcoWatt Device - Milestone 2

## Project Overview
This project implements a Modbus RTU Protocol Adapter and Acquisition Scheduler for the EcoWatt Device, designed for microcontroller deployment with robust error handling and optimized memory usage.

## Architecture

### Core Modules
- **config.h**: Centralized configuration with all constants and PROGMEM data
- **error_handler**: Comprehensive error management with retry logic and watchdog support
- **modbus_handler**: Unified Modbus frame processing, CRC validation, and exception handling
- **scheduler**: Task-based periodic operations with circular buffer storage
- **api_client**: HTTP client with timeout handling and exponential backoff retry
- **wifi_manager**: WiFi connection management with reconnection support

### Key Features implemented
- 5-second polling of voltage/current registers (0x0000-0x0002)
- Write operations to export power register (0x0008) every 25 seconds
- Comprehensive Modbus exception code handling (0x83, 0x86, etc.)
- CRC validation for all incoming/outgoing frames
- HTTP timeout and retry logic with exponential backoff
- WiFi disconnection detection and automatic reconnection
- Watchdog timer support for system reliability
- Memory-optimized circular buffer for data storage
- PROGMEM usage for constants to minimize RAM usage


## Configuration
Update `lib/config/config.h` with your specific:
- WiFi credentials
- API base URL and authentication key
- Polling intervals and retry parameters

#### Configuration Management
- **Centralized Constants**
  - All timing parameters in `config.h`
  - HTTP timeout and retry configurations
  - Modbus protocol parameters
  - System health check intervals

# Project Code Summary

## File Summaries

### src/
- **main.cpp**: Entry point that initializes all modules (WiFi, scheduler, error handler) and runs the main loop with periodic task scheduling.

### lib/config/
- **config.h**: Centralized configuration file containing all constants, API credentials, timing parameters, and PROGMEM data arrays.

### lib/wifi_manager/
- **wifi_manager.cpp/h**: Manages WiFi connection establishment, reconnection logic, and connection status monitoring.

### lib/api_client/
- **api_client.cpp/h**: HTTP client for sending Modbus frames to the remote API with retry logic, timeout handling, and exponential backoff.

### lib/error_handler/
- **error_handler.cpp/h**: Centralized error logging system with watchdog timer support and system health monitoring.

### lib/modbus_handler/
- **modbus_handler.cpp/h**: Core Modbus protocol implementation including frame generation, CRC calculation/validation, response parsing, and exception handling.

### lib/scheduler/
- **scheduler.cpp/h**: Task-based scheduler that manages periodic operations (read/write) and stores data in circular buffer with timestamp tracking.

### lib/calculateCRC/ & lib/checkCRC/
- **calculateCRC.cpp/h**: CRC-16 calculation functions for Modbus frame integrity.  
- **checkCRC.cpp/h**: CRC validation functions for incoming Modbus responses.

### lib/decoder/ (Legacy)
- **decoder.cpp/h**: Wrapper functions around modbus_handler (can be removed as it duplicates functionality).

### modbus_api/
- **app.py**: API controller for the Modbus API.
- **middleware.py**: Validation functions (API Key & CRC)
- **data.csv**: Local database file

---

## Overall Workflow

1. **System Initialization**: Main starts up, initializes error handler, scheduler, WiFi connection, and API client.  
2. **Task Scheduling**: Scheduler runs periodic tasks every 100ms checking for due operations.  
3. **Register Reading**: Every 5 seconds, read voltage/current registers (0x0000–0x0002).  
4. **Frame Generation**: Modbus handler creates request frame with slave address, function code, and register addresses.  
5. **CRC Addition**: Calculate and append CRC-16 checksum to ensure frame integrity.  
6. **API Request**: Send frame to remote API via HTTP POST with retry logic and timeout handling.  
7. **Response Parsing**: Validate received response, check CRC, and handle any Modbus exception codes.  
8. **Data Storage**: Store valid register values in circular buffer with timestamps.  
9. **Register Writing**: Every 25 seconds, write export power percentage to register 0x0008.  
10. **Health Monitoring**: Continuous WiFi status checking, error logging, and watchdog timer feeding.  
11. **Loop Continuation**: Return to step 2 and repeat the cycle indefinitely.  



The correct order is CRC, then Encryption, then MAC.
1. Append CRC to the payload
2. Encrypt the payload using function ```String encodeBase64(const uint8_t* payload, size_t length);```
3. Generate MAC using function ```String generateMAC(const String& encodedPayload);```

in API use the UPLOAD_PSK to compute and validate MAC

when uploading to the cloud add new headers as below
```json
{
  "nonce": <nonce value>,
  "payload_is_mock_encrypted": true,
  "payload": "<base64-encoded-JSON-string>",
  "mac": "Generated MAC"
}
```
