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

