# 🔧 Complete Write Command Execution Flow - Register 0x0008

## 📋 **Overview**
This document traces the **complete execution path** when the cloud sends a write command for register `0x0008` (the writable register among `0x0000` to `0x0009`).

---

## 🎯 **Target Register Information**

```cpp
// From scheduler.cpp, line 46
const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT] = {
    0x0000,  // Read-only
    0x0001,  // Read-only
    0x0002,  // Read-only
    0x0003,  // Read-only
    0x0004,  // Read-only
    0x0005,  // Read-only
    0x0006,  // Read-only
    0x0007,  // Read-only
    0x0008,  // ✅ WRITABLE REGISTER (EXPORT_POWER_REGISTER)
    0x0009   // Read-only
};

// From config.h
#define EXPORT_POWER_REGISTER 8   // Index in array = 8, actual register = 0x0008
#define MIN_EXPORT_POWER 0
#define MAX_EXPORT_POWER 100
```

**Note:** Register `0x0008` is typically used for **export power control** in solar inverters.

---

## 🔄 **COMPLETE EXECUTION FLOW**

### **Phase 1: Cloud Sends Command**

#### **Step 1.1: Device Uploads Data**
```cpp
// File: scheduler.cpp, execute_upload_task()
// Line: 437

String response = upload_api_send_request_with_retry(url, method, api_key, 
                                                      final_payload, final_payload_len, 
                                                      String(nonce), mac);
```

**HTTP Request:**
```
POST /api/cloud/write
Headers:
  - Content-Type: application/octet-stream
  - encryption: aes-256-cbc
  - nonce: 12345
  - mac: <HMAC-SHA256>
Body: <AES encrypted data>
```

#### **Step 1.2: Cloud Responds with Command**
```json
{
  "status": "success",
  "command": {
    "action": "write_register",
    "target_register": "8",
    "value": 50
  }
}
```

**Example:** Set export power to 50% (register 0x0008 = 50)

---

### **Phase 2: Command Parsing**

#### **Step 2.1: Extract Command from Response**
```cpp
// File: scheduler.cpp, line 443-461
// Function: execute_upload_task()

if (response.length() > 0) {
    String action;
    uint16_t reg = 0;
    uint16_t val = 0;

    if (extract_command(response, action, reg, val)) {
        Serial.println(F("[COMMAND] Command detected in cloud response"));
        
        if (action.equalsIgnoreCase("write_register")) {
            Serial.println(F("[COMMAND] Validating WRITE command"));

            // Store command atomically
            current_command.pending = true;
            current_command.register_address = reg;
            current_command.value = val;
            
            tasks[TASK_WRITE_REGISTER].enabled = true;
            tasks[TASK_COMMAND_HANDLING].enabled = true;
        }
    }
}
```

**Called Function:** `extract_command()`

#### **Step 2.2: Parse JSON Command**
```cpp
// File: command_parse.cpp, line 4-68
// Function: extract_command()

bool extract_command(const String& response, String& action, 
                    uint16_t& target_register, uint16_t& value) {
    
    // Find "command" block
    int cmd_index = response.indexOf(F("\"command\""));
    if (cmd_index >= 0) {
        
        // Extract 'action' field
        int action_start = response.indexOf(F("\"action\":\""), cmd_index);
        action = substring_between_quotes;  // "write_register"
        
        // Validate action
        if (!(action.equalsIgnoreCase("write_register") ||
              action.equalsIgnoreCase("read_register"))) {
            return false;  // Unsupported action
        }
        
        // Extract 'target_register' (STRING → uint16_t)
        int target_start = response.indexOf(F("\"target_register\":\""), cmd_index);
        String target_reg = substring_between_quotes;  // "8"
        target_register = target_reg.toInt();          // 8
        
        // Extract 'value' (NUMBER)
        if (action.equalsIgnoreCase("write_register")) {
            int value_start = response.indexOf(F("\"value\":"), cmd_index);
            String value_str = substring_until_comma_or_brace;  // "50"
            value = value_str.toInt();                          // 50
        }
        
        return true;
    }
    return false;
}
```

**Parsing Result:**
- `action` = `"write_register"`
- `target_register` = `8` (uint16_t)
- `value` = `50` (uint16_t)

#### **Step 2.3: Store Command**
```cpp
// File: scheduler.cpp, line 450-456
// Structure: command_state_t

typedef struct {
    bool pending;              // Command ready for execution
    uint16_t register_address; // Target register
    uint16_t value;           // Value to write
} command_state_t;

// Store parsed values
current_command.pending = true;
current_command.register_address = 8;    // ← Register 0x0008
current_command.value = 50;              // ← Value to write
```

#### **Step 2.4: Enable Write Task**
```cpp
// File: scheduler.cpp, line 454-455

tasks[TASK_WRITE_REGISTER].enabled = true;      // Execute write
tasks[TASK_COMMAND_HANDLING].enabled = true;    // Report result later
```

---

### **Phase 3: Write Task Execution**

#### **Step 3.1: Scheduler Triggers Write Task**
```cpp
// File: scheduler.cpp, line 48-87
// Function: scheduler_run()

void scheduler_run(void) {
    unsigned long current_time = millis();
    
    for (int i = 0; i < TASK_COUNT; i++) {
        if (!tasks[i].enabled) continue;
        
        if (current_time - tasks[i].last_run_ms >= tasks[i].interval_ms) {
            switch (tasks[i].type) {
                case TASK_WRITE_REGISTER:
                    execute_write_task();  // ← Called when interval elapsed
                    break;
            }
        }
    }
}
```

**Timing:** Write task runs at `WRITE_INTERVAL_MS = 7.5 seconds` (half of upload interval)

#### **Step 3.2: Execute Write Task**
```cpp
// File: scheduler.cpp, line 216-266
// Function: execute_write_task()

void execute_write_task(void) {
    Serial.println(F("Executing write task..."));
    
    // Check if command is pending
    if (!current_command.pending) {
        Serial.println(F("[WRITE] No pending command - skipping"));
        tasks[TASK_WRITE_REGISTER].enabled = false;
        return;
    }
    
    // Extract command values
    uint16_t export_power_value = current_command.value;         // 50
    uint16_t target_register = current_command.register_address;  // 8
    
    // Validate register and value
    if (!is_valid_write_value(target_register, export_power_value)) {
        log_error(ERROR_INVALID_REGISTER, "Invalid export power value");
        finalize_command("Failed - Invalid value");
        return;
    }

    // Format Modbus request frame
    String frame = format_request_frame(SLAVE_ADDRESS,           // 0x11 (17)
                                       FUNCTION_CODE_WRITE,      // 0x06
                                       target_register,          // 8
                                       export_power_value);      // 50

    // Add CRC
    frame = append_crc_to_frame(frame);
    
    // Send to Modbus API
    String url = API_BASE_URL + "/api/inverter/write";
    String response = api_send_request_with_retry(url, method, api_key, frame);
    
    // Process response
    if (response.length() > 0) {
        if (validate_modbus_response(response)) {
            if (is_exception_response(response)) {
                uint8_t exception_code = get_exception_code(response);
                finalize_command("Failed - Exception");
            } else {
                Serial.println(F("Write successful"));
                finalize_command("Success");
            }
        } else {
            finalize_command("Failed - Invalid response");
        }
    } else {
        finalize_command("Failed - No response");
    }
}
```

---

### **Phase 4: Value Validation**

#### **Step 4.1: Validate Write Value**
```cpp
// File: modbus_handler.cpp, line 56-69
// Function: is_valid_write_value()

bool is_valid_write_value(uint16_t register_addr, uint16_t value) {
    // Check if register exists
    if (!is_valid_register(register_addr)) {
        return false;  // Register address out of range
    }
    
    // Special validation for EXPORT_POWER_REGISTER (register 8)
    if (register_addr == EXPORT_POWER_REGISTER) {  // 8
        return (value >= MIN_EXPORT_POWER &&       // >= 0
                value <= MAX_EXPORT_POWER);        // <= 100
    }
    
    // General validation for other registers
    return true;  // Allow full uint16_t range (0-65535)
}
```

**Validation Logic:**
```cpp
// File: modbus_handler.cpp, line 52-54

bool is_valid_register(uint16_t register_addr) {
    return register_addr < MAX_REGISTERS;  // Must be < 10
}

// For register 8 (EXPORT_POWER_REGISTER):
// ✅ VALID:   0 <= value <= 100
// ❌ INVALID: value > 100 or value < 0
```

**Example Validation:**
```
register_address = 8, value = 50
→ is_valid_register(8)? → 8 < 10 → YES ✅
→ register_addr == EXPORT_POWER_REGISTER? → 8 == 8 → YES
→ value >= MIN_EXPORT_POWER? → 50 >= 0 → YES ✅
→ value <= MAX_EXPORT_POWER? → 50 <= 100 → YES ✅
→ VALIDATION PASSED ✅
```

---

### **Phase 5: Modbus Frame Formatting**

#### **Step 5.1: Format Request Frame**
```cpp
// File: modbus_handler.cpp, line 118-126
// Function: format_request_frame()

String format_request_frame(uint8_t slave_addr,        // 0x11 (17)
                           uint8_t function_code,      // 0x06 (Write Single Register)
                           uint16_t start_reg,         // 8
                           uint16_t count_or_value) {  // 50
    char frame[17];  // 8 bytes = 16 hex chars + null terminator
    
    snprintf(frame, sizeof(frame), "%02X%02X%04X%04X", 
             slave_addr,        // 11
             function_code,     // 06
             start_reg,         // 0008
             count_or_value);   // 0032 (50 in hex)
    
    return String(frame);
}
```

**Generated Frame (without CRC):**
```
11 06 0008 0032
│  │  │    │
│  │  │    └─ Value: 50 (0x0032)
│  │  └────── Register: 8 (0x0008)
│  └───────── Function Code: 0x06 (Write Single Register)
└──────────── Slave Address: 0x11 (17 decimal)

Hex string: "1106000800032"
```

#### **Step 5.2: Append CRC**
```cpp
// File: modbus_handler.cpp, line 128-146
// Function: append_crc_to_frame()

String append_crc_to_frame(const String& frame_without_crc) {
    // Convert hex string to bytes
    uint8_t frame_bytes[] = {0x11, 0x06, 0x00, 0x08, 0x00, 0x32};
    
    // Calculate CRC-16 (Modbus)
    uint16_t crc = calculateCRC(frame_bytes, 6);  // e.g., 0xAB12
    
    // Append CRC (LOW byte first - Modbus convention)
    char crc_hex[5];
    snprintf(crc_hex, sizeof(crc_hex), "%02X%02X", 
             crc & 0xFF,         // Low byte: 0x12
             (crc >> 8) & 0xFF); // High byte: 0xAB
    
    return frame_without_crc + String(crc_hex);
}
```

**Final Frame with CRC:**
```
11 06 0008 0032 12AB
│  │  │    │    │
│  │  │    │    └─ CRC: 0x12AB (example, low byte first)
│  │  │    └────── Value: 50
│  │  └─────────── Register: 8
│  └────────────── Function Code: 0x06
└───────────────── Slave Address: 0x11

Hex string: "110600080003212AB"
```

---

### **Phase 6: Send to Modbus API**

#### **Step 6.1: Send HTTP Request**
```cpp
// File: scheduler.cpp, line 238-244

String url = API_BASE_URL + "/api/inverter/write";  // "http://192.168.1.100:5000/api/inverter/write"
String method = "POST";
String api_key = API_KEY;

String response = api_send_request_with_retry(url, method, api_key, frame);
```

**HTTP Request to Modbus API:**
```http
POST /api/inverter/write HTTP/1.1
Host: 192.168.1.100:5000
Content-Type: application/json
Authorization: <API_KEY>

{
  "frame": "110600080003212AB"
}
```

#### **Step 6.2: Modbus API Processes Request**
```
Modbus API Server:
1. Receives HTTP request
2. Extracts Modbus frame: "110600080003212AB"
3. Converts to bytes: [0x11, 0x06, 0x00, 0x08, 0x00, 0x32, 0x12, 0xAB]
4. Verifies CRC
5. Sends via RS485/TCP to inverter:
   - Slave Address: 0x11
   - Function Code: 0x06 (Write Single Register)
   - Register Address: 0x0008
   - Value: 0x0032 (50 decimal)
   - CRC: 0x12AB
6. Receives response from inverter
7. Returns HTTP response
```

---

### **Phase 7: Process Modbus Response**

#### **Step 7.1: Validate Response**
```cpp
// File: scheduler.cpp, line 246-261

if (response.length() > 0) {
    if (validate_modbus_response(response)) {
        // Response is valid
    }
}
```

**Called Function:**
```cpp
// File: modbus_handler.cpp, line 7-27
// Function: validate_modbus_response()

bool validate_modbus_response(const String& response) {
    // Check minimum length
    if (response.length() < 6) {
        return false;
    }
    
    // Check even length (hex pairs)
    if (response.length() % 2 != 0) {
        return false;
    }
    
    // Verify CRC
    if (!verify_frame_crc(response)) {
        log_error(ERROR_CRC_FAILED, "CRC validation failed");
        return false;
    }
    
    return true;
}
```

#### **Step 7.2: Check for Exceptions**
```cpp
// File: scheduler.cpp, line 247-253

if (is_exception_response(response)) {
    uint8_t exception_code = get_exception_code(response);
    finalize_command("Failed - Exception");
}
```

**Exception Detection:**
```cpp
// File: modbus_handler.cpp, line 29-40

bool is_exception_response(const String& response) {
    // Extract function code (second byte)
    String func_code_str = response.substring(2, 4);
    uint8_t func_code = strtoul(func_code_str.c_str(), nullptr, 16);
    
    // Exception responses have bit 7 set
    return (func_code & 0x80) != 0;  // 0x06 vs 0x86
}
```

**Success Response Format:**
```
11 06 0008 0032 CRC
│  │  │    │
│  │  │    └─ Echoed value: 50
│  │  └────── Echoed register: 8
│  └───────── Echoed function code: 0x06
└──────────── Echoed slave address: 0x11
```

**Exception Response Format:**
```
11 86 01 CRC
│  │  │
│  │  └─ Exception code (e.g., 0x01 = Illegal Function)
│  └──── Function code with bit 7 set: 0x86 (0x06 | 0x80)
└─────── Slave address: 0x11
```

#### **Step 7.3: Finalize Command**
```cpp
// File: scheduler.cpp, line 254-259

if (!is_exception_response(response)) {
    Serial.print(F("Write successful: Register "));
    Serial.print(target_register);  // 8
    Serial.print(F(" set to "));
    Serial.println(export_power_value);  // 50
    finalize_command("Success");
}
```

---

### **Phase 8: Command Finalization**

#### **Step 8.1: Store Result**
```cpp
// File: scheduler.cpp, line 667-677
// Function: finalize_command()

void finalize_command(const String& status) {
    write_status = status;                            // "Success"
    write_executed_timestamp = get_current_timestamp(); // "2025-10-16T14:30:45Z"
    
    current_command.pending = false;                  // Clear pending flag
    
    tasks[TASK_WRITE_REGISTER].enabled = false;       // Disable write task
    tasks[TASK_COMMAND_HANDLING].enabled = true;      // Enable result reporting
    
    Serial.print(F("[COMMAND] Finalized with status: "));
    Serial.println(status);
}
```

**Result Storage:**
- `write_status` = `"Success"` (or error message)
- `write_executed_timestamp` = ISO 8601 timestamp
- `current_command.pending` = `false`

---

### **Phase 9: Result Reporting**

#### **Step 9.1: Scheduler Triggers Command Task**
```cpp
// File: scheduler.cpp, line 48-87
// Function: scheduler_run()

void scheduler_run(void) {
    // ... after some time (upload_interval_ms) ...
    
    if (tasks[TASK_COMMAND_HANDLING].enabled) {
        execute_command_task();  // Report result to cloud
    }
}
```

#### **Step 9.2: Send Result to Cloud**
```cpp
// File: scheduler.cpp, line 560-589
// Function: execute_command_task()

void execute_command_task(void) {
    if (write_status.length() == 0) {
        tasks[TASK_COMMAND_HANDLING].enabled = false;
        return;
    }

    // Format result JSON
    String frame = "{\"command_result\":{";
    frame += "\"status\":\"" + write_status + "\",";            // "Success"
    frame += "\"executed_at\":\"" + write_executed_timestamp + "\"";  // Timestamp
    frame += "\"}}";

    frame = append_crc_to_frame(frame);
    
    // Send to cloud
    String url = UPLOAD_API_BASE_URL + "/api/cloud/command_result";
    api_command_request_with_retry(url, method, api_key, frame);

    // Clear result
    write_status = "";
    write_executed_timestamp = "";
    tasks[TASK_COMMAND_HANDLING].enabled = false;
}
```

**HTTP Request to Cloud:**
```http
POST /api/cloud/command_result HTTP/1.1
Content-Type: application/json
Authorization: <UPLOAD_API_KEY>

{
  "command_result": {
    "status": "Success",
    "executed_at": "2025-10-16T14:30:45Z"
  }
}
```

---

## 📊 **COMPLETE EXECUTION TIMELINE**

```
T=0s        Device uploads data
            ↓
T=0.5s      Cloud responds with command:
            {
              "status": "success",
              "command": {
                "action": "write_register",
                "target_register": "8",
                "value": 50
              }
            }
            ↓
T=0.6s      extract_command() parses JSON
            - action = "write_register"
            - target_register = 8
            - value = 50
            ↓
T=0.6s      Store in current_command:
            - current_command.pending = true
            - current_command.register_address = 8
            - current_command.value = 50
            ↓
T=0.6s      Enable tasks:
            - TASK_WRITE_REGISTER.enabled = true
            - TASK_COMMAND_HANDLING.enabled = true
            ↓
T=7.5s      Scheduler triggers execute_write_task()
            (WRITE_INTERVAL_MS = 7500ms)
            ↓
T=7.5s      Validate value:
            - is_valid_write_value(8, 50)
            - is_valid_register(8)? → 8 < 10 → YES
            - 8 == EXPORT_POWER_REGISTER? → YES
            - 50 >= 0 && 50 <= 100? → YES
            - VALIDATION PASSED ✅
            ↓
T=7.5s      Format Modbus frame:
            - format_request_frame(0x11, 0x06, 8, 50)
            - Generated: "1106000800032"
            ↓
T=7.5s      Add CRC:
            - append_crc_to_frame("1106000800032")
            - CRC calculated: e.g., 0x12AB
            - Final frame: "110600080003212AB"
            ↓
T=7.6s      Send to Modbus API:
            POST /api/inverter/write
            { "frame": "110600080003212AB" }
            ↓
T=7.7s      Modbus API sends via RS485:
            [0x11, 0x06, 0x00, 0x08, 0x00, 0x32, 0x12, 0xAB]
            ↓
T=7.8s      Inverter processes write:
            - Slave address matches: 0x11 ✅
            - Function code: 0x06 (Write Single Register) ✅
            - Register address: 0x0008 ✅
            - Value: 0x0032 (50 decimal) ✅
            - CRC valid ✅
            - Write to internal register 0x0008 = 50
            - Export power set to 50%
            ↓
T=7.9s      Inverter responds:
            [0x11, 0x06, 0x00, 0x08, 0x00, 0x32, CRC]
            (Echoes request if successful)
            ↓
T=8.0s      Device receives response:
            "110600080003212AB"
            ↓
T=8.0s      Validate response:
            - validate_modbus_response() ✅
            - CRC valid ✅
            - is_exception_response()? → NO ✅
            - SUCCESS!
            ↓
T=8.0s      finalize_command("Success"):
            - write_status = "Success"
            - write_executed_timestamp = "2025-10-16T14:30:45Z"
            - current_command.pending = false
            - TASK_WRITE_REGISTER.enabled = false
            - TASK_COMMAND_HANDLING.enabled = true
            ↓
T=15s       Scheduler triggers execute_command_task()
            (COMMAND_INTERVAL_MS = 15000ms, coupled to upload)
            ↓
T=15.1s     Send result to cloud:
            POST /api/cloud/command_result
            {
              "command_result": {
                "status": "Success",
                "executed_at": "2025-10-16T14:30:45Z"
              }
            }
            ↓
T=15.5s     Cloud receives result:
            - Logs successful write
            - Updates database
            - Result lifecycle COMPLETE ✅
```

---

## 🔍 **FUNCTION CALL HIERARCHY**

```
scheduler_run()
└── execute_upload_task()
    ├── upload_api_send_request_with_retry()
    │   └── [Receives cloud response with command]
    │
    └── extract_command(response, action, reg, val)
        ├── response.indexOf("command")
        ├── response.indexOf("action")
        ├── response.indexOf("target_register")
        └── response.indexOf("value")
        └── [Returns: action="write_register", reg=8, val=50]
        
        → current_command.pending = true
        → current_command.register_address = 8
        → current_command.value = 50
        → tasks[TASK_WRITE_REGISTER].enabled = true

[Time passes: WRITE_INTERVAL_MS]

scheduler_run()
└── execute_write_task()
    ├── is_valid_write_value(target_register, export_power_value)
    │   ├── is_valid_register(8)
    │   │   └── return 8 < MAX_REGISTERS (8 < 10) → true
    │   └── if (register_addr == EXPORT_POWER_REGISTER)
    │       └── return (50 >= 0 && 50 <= 100) → true
    │
    ├── format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_WRITE, 8, 50)
    │   └── snprintf("%02X%02X%04X%04X", 0x11, 0x06, 0x0008, 0x0032)
    │       └── return "1106000800032"
    │
    ├── append_crc_to_frame("1106000800032")
    │   ├── Convert hex string to bytes
    │   ├── calculateCRC([0x11, 0x06, 0x00, 0x08, 0x00, 0x32], 6)
    │   │   └── return 0x12AB (example)
    │   └── Append CRC low byte first: "12AB"
    │       └── return "110600080003212AB"
    │
    ├── api_send_request_with_retry(url, method, api_key, frame)
    │   └── POST to /api/inverter/write
    │       └── [Receives Modbus response]
    │
    ├── validate_modbus_response(response)
    │   ├── Check length >= 6
    │   ├── Check even length
    │   └── verify_frame_crc(response)
    │       └── checkCRC(response) → true
    │
    ├── is_exception_response(response)
    │   ├── Extract function code (byte 2)
    │   └── Check if (func_code & 0x80) != 0
    │       └── return false (0x06 & 0x80 = 0)
    │
    └── finalize_command("Success")
        ├── write_status = "Success"
        ├── write_executed_timestamp = get_current_timestamp()
        ├── current_command.pending = false
        ├── tasks[TASK_WRITE_REGISTER].enabled = false
        └── tasks[TASK_COMMAND_HANDLING].enabled = true

[Time passes: COMMAND_INTERVAL_MS]

scheduler_run()
└── execute_command_task()
    ├── Format JSON result
    ├── append_crc_to_frame(json)
    ├── api_command_request_with_retry(url, method, api_key, frame)
    │   └── POST to /api/cloud/command_result
    └── Clear write_status and disable task
```

---

## 📝 **KEY VARIABLES TRACKING**

### **During Command Reception:**
```cpp
// Global variables
String action;                          // "write_register"
uint16_t reg;                          // 8
uint16_t val;                          // 50

// Command state
current_command.pending = true;
current_command.register_address = 8;
current_command.value = 50;

// Task enablement
tasks[TASK_WRITE_REGISTER].enabled = true;
tasks[TASK_COMMAND_HANDLING].enabled = true;
```

### **During Write Execution:**
```cpp
// Local variables in execute_write_task()
uint16_t export_power_value = current_command.value;         // 50
uint16_t target_register = current_command.register_address;  // 8

// Modbus frame components
uint8_t slave_addr = SLAVE_ADDRESS;      // 0x11 (17)
uint8_t function_code = FUNCTION_CODE_WRITE;  // 0x06
uint16_t start_reg = target_register;    // 8
uint16_t count_or_value = export_power_value;  // 50

// Generated frame
String frame = "1106000800032";          // Without CRC
String frame_with_crc = "110600080003212AB";  // With CRC
```

### **During Finalization:**
```cpp
// Result storage
write_status = "Success";
write_executed_timestamp = "2025-10-16T14:30:45Z";

// Command state
current_command.pending = false;
current_command.register_address = 0;
current_command.value = 0;

// Task state
tasks[TASK_WRITE_REGISTER].enabled = false;
tasks[TASK_COMMAND_HANDLING].enabled = true;
```

---

## 🎯 **SUMMARY**

### **Register 0x0008 Write Flow:**

1. **Cloud sends command** → `{"action":"write_register", "target_register":"8", "value":50}`
2. **Device parses command** → `extract_command()` → Stores in `current_command`
3. **Validates value** → `is_valid_write_value(8, 50)` → Checks 0 ≤ 50 ≤ 100 ✅
4. **Formats Modbus frame** → `format_request_frame(0x11, 0x06, 8, 50)` → `"1106000800032"`
5. **Adds CRC** → `append_crc_to_frame()` → `"110600080003212AB"`
6. **Sends to Modbus API** → POST `/api/inverter/write`
7. **Inverter writes register** → Register 0x0008 = 50 (export power 50%)
8. **Validates response** → `validate_modbus_response()` → Success ✅
9. **Finalizes command** → `finalize_command("Success")`
10. **Reports result to cloud** → POST `/api/cloud/command_result` → `{"status":"Success"}`

**Total Time:** ~15 seconds from command reception to result reporting

---

**Document Date:** October 16, 2025  
**Status:** ✅ **COMPLETE FLOW DOCUMENTED**  
**Target Register:** 0x0008 (EXPORT_POWER_REGISTER)
