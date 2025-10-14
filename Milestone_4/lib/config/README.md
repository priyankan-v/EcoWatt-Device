# Configuration Manager

A centralized configuration management system for the EcoWatt Device ESP32 firmware that handles runtime configuration updates, persistent storage, and thread-safe parameter access.

## Features

- **Runtime Configuration**: Update device parameters without restart
- **Persistent Storage**: Configuration saved to ESP32 NVS (same pattern as FOTA)
- **Thread-Safe Access**: Mutex-protected configuration operations
- **JSON Message Format**: Standardized configuration update protocol
- **Parameter Validation**: Range checking and type validation
- **HTTP Interface**: Simple HTTP endpoints for configuration management

## Architecture

### Core Components

1. **ConfigManager**: Main configuration class with NVS storage
2. **ConfigHandler**: HTTP server for remote configuration updates
3. **Scheduler Integration**: Dynamic task interval updates

### Files

- `config_manager.h/cpp`: Core configuration management
- `config_handler.h/cpp`: HTTP interface for remote updates
- `config.h`: Static configuration constants (unchanged)

## Usage

### Initialization

```cpp
#include <config_manager.h>
#include <config_handler.h>

void setup() {
    // Initialize WiFi first
    wifi_init();
    
    // Initialize ConfigManager
    if (!config_manager_init()) {
        Serial.println("ConfigManager initialization failed");
    }
    
    // Initialize HTTP handler (optional)
    if (!config_handler_init(8080)) {
        Serial.println("ConfigHandler initialization failed");
    }
}

void loop() {
    scheduler_run();           // Scheduler uses dynamic config
    config_handler_process(); // Handle HTTP requests
}
```

### Accessing Configuration

```cpp
// Get current configuration
runtime_config_t config = config_get_current();

// Get specific parameters
uint32_t sampling_ms = config_get_sampling_interval_ms();
uint32_t upload_ms = config_get_upload_interval_ms();
uint8_t slave_addr = config_get_slave_address();
uint8_t reg_count = config_get_register_count();

// Get active registers
uint16_t registers[MAX_REGISTERS];
config_get_active_registers(registers, MAX_REGISTERS);
```

## Remote Configuration

### HTTP Interface

**Base URL**: `http://[device_ip]:8080`

#### Update Configuration
```http
POST /config
Content-Type: application/json

{
  "config_update": {
    "sampling_interval": 5,
    "upload_interval": 900,
    "registers": ["voltage", "current", "power", "frequency"],
    "slave_address": 17
  }
}
```

#### Get Status
```http
GET /config
```

Response:
```json
{
  "status": {
    "device_id": "AA:BB:CC:DD:EE:FF",
    "uptime_sec": 12345,
    "free_heap": 234567,
    "wifi_rssi": -45,
    "local_ip": "192.168.1.100",
    "configuration": {
      "sampling_interval": 5,
      "upload_interval": 900,
      "slave_address": 17,
      "register_count": 4,
      "config_valid": true,
      "registers": ["0x0", "0x1", "0x2", "0x4"]
    }
  }
}
```

### Message Format

**Cloud → Device**:
```json
{
  "config_update": {
    "sampling_interval": 5,
    "registers": ["voltage", "current", "frequency"]
  }
}
```

**Device → Cloud**:
```json
{
  "config_ack": {
    "accepted": ["sampling_interval", "registers"],
    "rejected": [],
    "unchanged": []
  }
}
```

## Supported Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `sampling_interval` | uint32 | 1-3600 seconds | Time between register reads |
| `upload_interval` | uint32 | 5-86400 seconds | Time between data uploads |
| `registers` | array | 1-10 strings | Register names to read |
| `slave_address` | uint8 | 1-247 | Modbus slave address |

### Supported Register Names

- `voltage` (0x0000)
- `current` (0x0001)
- `power` (0x0002)
- `energy` (0x0003)
- `frequency` (0x0004)
- `power_factor` (0x0005)
- `temperature` (0x0006)
- `humidity` (0x0007)
- `export_power` (0x0008)
- `import_power` (0x0009)

## Integration with Existing System

### Scheduler Integration

The scheduler automatically uses dynamic configuration:

```cpp
void scheduler_run(void) {
    // Update intervals from ConfigManager
    if (g_config_manager && g_config_manager->is_initialized()) {
        tasks[TASK_READ_REGISTERS].interval_ms = config_get_sampling_interval_ms();
        tasks[TASK_UPLOAD_DATA].interval_ms = config_get_upload_interval_ms();
    }
    // ... rest of scheduler logic
}
```

### Modbus Task Integration

```cpp
void execute_read_task(void) {
    // Get current configuration
    uint8_t slave_addr = config_get_slave_address();
    uint8_t register_count = config_get_register_count();
    uint16_t active_registers[MAX_REGISTERS];
    config_get_active_registers(active_registers, MAX_REGISTERS);
    
    // Use configured parameters
    String frame = format_request_frame(slave_addr, FUNCTION_CODE_READ, 
                                      active_registers[0], register_count);
    // ... rest of read logic
}
```

## Persistent Storage

Configuration is automatically saved to ESP32 NVS using the same pattern as FOTA:

- **Namespace**: `device_config`
- **Keys**: `sampling_ms`, `upload_ms`, `slave_addr`, `reg_count`, `registers`
- **Storage**: Automatic save on configuration changes
- **Recovery**: Automatic load on boot with fallback to defaults

## Thread Safety

All configuration access is protected by FreeRTOS mutexes:

```cpp
// All public methods are thread-safe
uint32_t interval = config_get_sampling_interval_ms(); // Safe from any task
config_status_t result = config_apply_update(json);   // Safe concurrent updates
```

## Error Handling

Configuration operations return status codes:

- `CONFIG_SUCCESS` (0): Operation successful
- `CONFIG_ERROR_INVALID_PARAM` (1): Invalid parameter name
- `CONFIG_ERROR_OUT_OF_RANGE` (2): Parameter value out of range
- `CONFIG_ERROR_STORAGE_FAILED` (3): Failed to save to NVS
- `CONFIG_ERROR_JSON_PARSE` (4): Invalid JSON format
- `CONFIG_ERROR_UNCHANGED` (5): No changes detected

## Testing

### Using curl

```bash
# Get current status
curl http://192.168.1.100:8080/config

# Update configuration
curl -X POST http://192.168.1.100:8080/config \
  -H "Content-Type: application/json" \
  -d '{
    "config_update": {
      "sampling_interval": 5,
      "registers": ["voltage", "current", "power"]
    }
  }'
```

### Using Python

```python
import requests
import json

device_ip = "192.168.1.100"
base_url = f"http://{device_ip}:8080"

# Get status
response = requests.get(f"{base_url}/config")
print("Status:", json.dumps(response.json(), indent=2))

# Update configuration
config_update = {
    "config_update": {
        "sampling_interval": 5,
        "registers": ["voltage", "current", "frequency"]
    }
}

response = requests.post(f"{base_url}/config", json=config_update)
print("Update result:", json.dumps(response.json(), indent=2))
```

## Default Configuration

When no saved configuration exists:

```cpp
sampling_interval_ms: 3000  // 3 seconds (from POLL_INTERVAL_MS)
upload_interval_ms: 15000   // 15 seconds (from UPLOAD_INTERVAL_MS)
slave_address: 0x11         // (from SLAVE_ADDRESS)
register_count: 4
active_registers: [0x0000, 0x0001, 0x0002, 0x0004] // voltage, current, power, frequency
config_valid: true
```

## Dependencies

- **ArduinoJson**: JSON parsing and generation
- **Preferences**: ESP32 NVS storage
- **WebServer**: HTTP interface (ESP32 built-in)
- **WiFi**: Network connectivity

## Design Philosophy

This Configuration Manager follows the existing codebase patterns:

1. **Minimal Integration**: Extends existing config library without major changes
2. **NVS Pattern**: Uses same NVS approach as FOTA module
3. **Thread Safety**: FreeRTOS mutex protection like other modules
4. **Global Functions**: C-style API consistent with existing code
5. **Fallback Behavior**: Graceful degradation when ConfigManager unavailable

The system provides centralized configuration management while maintaining compatibility with the existing EcoWatt Device architecture.