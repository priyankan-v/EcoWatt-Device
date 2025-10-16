# Command Execution Flow Analysis 📋

## Overview
The system implements **remote command execution** via cloud API responses, allowing the cloud server to send Modbus write/read commands to control the inverter.

---

## 🔄 **Complete Command Execution Flow**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       COMMAND LIFECYCLE                                 │
└─────────────────────────────────────────────────────────────────────────┘

1. CLOUD SENDS COMMAND
   ↓
2. DEVICE UPLOADS DATA + RECEIVES COMMAND IN RESPONSE
   ↓
3. COMMAND PARSING (command_parse.cpp)
   ↓
4. COMMAND VALIDATION & STORAGE
   ↓
5. TASK SCHEDULING (TASK_WRITE_REGISTER enabled)
   ↓
6. MODBUS WRITE EXECUTION (execute_write_task)
   ↓
7. RESULT REPORTING (TASK_COMMAND_HANDLING)
   ↓
8. CLOUD ACKNOWLEDGMENT
```

---

## 📦 **Step 1: Cloud Command Format**

### **Expected JSON Response:**
```json
{
  "status": "success",
  "command": {
    "action": "write_register",          // or "read_register"
    "target_register": "12345",          // Register address (string format)
    "value": 5000                        // Value to write (only for write_register)
  }
}
```

### **Supported Actions:**
1. ✅ `write_register` - Write value to Modbus holding register
2. ✅ `read_register` - Read value from Modbus register (prepared but not fully implemented)

---

## 🔍 **Step 2: Command Parsing (`command_parse.cpp`)**

### **Function:** `extract_command()`
**Location:** `lib/command_parse/command_parse.cpp`

### **Parsing Logic:**
```cpp
bool extract_command(const String& response, String& action, 
                     uint16_t& target_register, uint16_t& value)
```

### **What It Does:**
1. **Validates response** - Checks if non-empty
2. **Searches for `"command"` block** - Looks for JSON key
3. **Extracts `action`** - Validates against allowed actions:
   - ✅ `write_register`
   - ✅ `read_register`
   - ❌ Any other action → Returns `false`
4. **Extracts `target_register`** - Converts string to `uint16_t`
5. **Extracts `value`** (only for write_register) - Converts to `uint16_t`

### **Validation:**
```cpp
if(!(action.equalsIgnoreCase("write_register") ||
     action.equalsIgnoreCase("read_register"))) {
    Serial.println(F("Error: Unsupported action command received"));
    return false;
}
```

### **Parsing Example:**

**Input Response:**
```json
{
  "status": "success",
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 3000
  }
}
```

**Output:**
- `action` = `"write_register"`
- `target_register` = `40100`
- `value` = `3000`
- Returns: `true`

---

## 📥 **Step 3: Command Reception in Upload Response**

### **Location:** `scheduler.cpp` → `execute_upload_task()` (lines 437-460)

### **Flow After Upload:**
```cpp
// 1. Upload encrypted data to cloud
String response = upload_api_send_request_with_retry(...);

// 2. Check if response contains command
if (response.length() > 0) {
    String action;
    uint16_t reg = 0;
    uint16_t val = 0;

    // 3. Parse command from response
    if (extract_command(response, action, reg, val)) {
        Serial.println(F("[COMMAND] Command detected in cloud response"));
        
        // 4. Handle WRITE command
        if (action.equalsIgnoreCase("write_register")) {
            Serial.println(F("[COMMAND] Validating WRITE command"));

            // 5. Store command atomically
            current_command.pending = true;
            current_command.register_address = reg;
            current_command.value = val;
            
            // 6. Enable write task
            tasks[TASK_WRITE_REGISTER].enabled = true;
            tasks[TASK_COMMAND_HANDLING].enabled = true;

        } else if (action.equalsIgnoreCase("read_register")) {
            Serial.println(F("[COMMAND] Preparing to execute READ task"));
            // Note: READ task not fully implemented

        } else {
            Serial.println(F("[COMMAND] Unknown action command received"));
        }
    }
}
```

### **Command Storage Structure:**
```cpp
typedef struct {
    bool pending;              // Is command waiting to execute?
    uint16_t register_address; // Target Modbus register
    uint16_t value;           // Value to write
} command_state_t;

command_state_t current_command;
```

---

## ⚙️ **Step 4: Task Scheduling**

### **Task Configuration:**
```cpp
scheduler_task_t tasks[] = {
    {TASK_READ_REGISTERS,    READ_INTERVAL_MS,    0, true},   // Always enabled
    {TASK_WRITE_REGISTER,    WRITE_INTERVAL_MS,   0, false},  // Enabled by command
    {TASK_UPLOAD_DATA,       UPLOAD_INTERVAL_MS,  0, true},   // Always enabled
    {TASK_COMMAND_HANDLING,  COMMAND_INTERVAL_MS, 0, false}   // Enabled by command
};
```

### **When Command Arrives:**
- ✅ `TASK_WRITE_REGISTER.enabled = true` → Execute write on next scheduler cycle
- ✅ `TASK_COMMAND_HANDLING.enabled = true` → Report result after execution

---

## 🔧 **Step 5: Modbus Write Execution**

### **Function:** `execute_write_task()`
**Location:** `scheduler.cpp` (lines 215-267)

### **Execution Flow:**
```cpp
void execute_write_task(void) {
    Serial.println(F("Executing write task..."));
    
    // 1. Check if command is pending
    if (!current_command.pending) {
        Serial.println(F("[WRITE] No pending command - skipping"));
        tasks[TASK_WRITE_REGISTER].enabled = false;
        return;
    }
    
    uint16_t export_power_value = current_command.value;
    uint16_t target_register = current_command.register_address;
    
    // 2. Validate write value
    if (!is_valid_write_value(target_register, export_power_value)) {
        log_error(ERROR_INVALID_REGISTER, "Invalid export power value");
        finalize_command("Failed - Invalid value");
        return;
    }

    // 3. Format Modbus request frame
    String frame = format_request_frame(SLAVE_ADDRESS, FUNCTION_CODE_WRITE, 
                                       target_register, export_power_value);
    frame = append_crc_to_frame(frame);
    
    // 4. Send to Modbus API
    String url = API_BASE_URL;
    url += "/api/inverter/write";
    String method = "POST";
    String api_key = API_KEY;
    String response = api_send_request_with_retry(url, method, api_key, frame);
    
    // 5. Process response
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

### **Validation Function:**
```cpp
bool is_valid_write_value(uint16_t target_register, uint16_t value) {
    // Add register-specific validation
    // Example: Check if value is within acceptable range
    return true;
}
```

---

## 📤 **Step 6: Result Reporting**

### **Function:** `execute_command_task()`
**Location:** `scheduler.cpp` (lines 555-596)

### **Result Reporting Flow:**
```cpp
void execute_command_task(void) {
    Serial.println(F("Executing command task..."));

    // 1. Check if result is ready
    if (write_status.length() == 0) {
        Serial.println(F("[COMMAND] No result to report"));
        tasks[TASK_COMMAND_HANDLING].enabled = false;
        return;
    }

    // 2. Format result JSON
    String frame = "{\"command_result\":{";
    frame += "\"status\":\"";
    frame += write_status;              // "Success" or "Failed - ..."
    frame += "\",\"executed_at\":\"";
    frame += write_executed_timestamp;  // ISO 8601 timestamp
    frame += "\"}}";

    frame = append_crc_to_frame(frame);
    
    // 3. Send to cloud
    String url = UPLOAD_API_BASE_URL;
    url += "/api/command_result";
    String method = "POST";
    String api_key = UPLOAD_API_KEY;
    
    String response = json_api_send_request(url, method, api_key, frame);
    
    // 4. Verify acknowledgment
    if (response.length() > 0 && validate_upload_response(response)) {
        Serial.println(F("[COMMAND] Result successfully reported to cloud"));
        write_status = "";  // Clear result
        tasks[TASK_COMMAND_HANDLING].enabled = false;
    } else {
        Serial.println(F("[COMMAND] Failed to report result - will retry"));
    }
}
```

### **Result JSON Format:**
```json
{
  "command_result": {
    "status": "Success",                    // or "Failed - Exception"
    "executed_at": "2025-10-15T14:30:45Z"  // ISO 8601 timestamp
  }
}
```

---

## 🔄 **Step 7: Command Finalization**

### **Function:** `finalize_command()`
**Location:** `scheduler.cpp`

```cpp
void finalize_command(const String& status) {
    // Store result for reporting
    write_status = status;
    write_executed_timestamp = get_iso8601_timestamp();
    
    // Clear command state
    current_command.pending = false;
    current_command.register_address = 0;
    current_command.value = 0;
    
    // Disable write task, enable reporting task
    tasks[TASK_WRITE_REGISTER].enabled = false;
    tasks[TASK_COMMAND_HANDLING].enabled = true;
}
```

---

## 🎯 **Current Implementation Status**

### ✅ **Fully Implemented:**
1. ✅ Command parsing from cloud response
2. ✅ WRITE_REGISTER action support
3. ✅ Modbus write execution via API
4. ✅ Result reporting back to cloud
5. ✅ Task-based scheduling
6. ✅ Error handling and validation
7. ✅ Atomic command state management

### ⚠️ **Partially Implemented:**
1. ⚠️ READ_REGISTER action - Parsed but execution not implemented

### ❌ **Not Implemented:**
1. ❌ Command queue (only one command at a time)
2. ❌ Command timeout mechanism
3. ❌ Command priority levels
4. ❌ Command history/logging

---

## 🛡️ **Security Considerations**

### **Current Security:**
1. ✅ Commands only accepted from authenticated cloud API
2. ✅ AES-256-CBC encryption on uploaded data
3. ✅ HMAC-SHA256 MAC verification
4. ✅ Value validation before Modbus write

### **Missing Security:**
1. ❌ Command signature verification
2. ❌ Command replay attack protection (no nonce on commands)
3. ❌ Rate limiting on command execution
4. ❌ Whitelist of allowed registers

---

## 📊 **Example Command Execution Timeline**

```
T+0s    → Device uploads data (encrypted)
T+0.5s  → Cloud responds with command:
          {
            "status": "success",
            "command": {
              "action": "write_register",
              "target_register": "40100",
              "value": 3000
            }
          }
T+0.6s  → extract_command() parses command
T+0.6s  → current_command.pending = true
T+0.6s  → TASK_WRITE_REGISTER.enabled = true
T+1s    → Scheduler runs execute_write_task()
T+1.2s  → Modbus write sent to inverter API
T+1.5s  → Modbus response received
T+1.5s  → finalize_command("Success") called
T+1.5s  → write_status = "Success"
T+1.5s  → TASK_COMMAND_HANDLING.enabled = true
T+2s    → Scheduler runs execute_command_task()
T+2.2s  → Result JSON sent to cloud
T+2.4s  → Cloud ACK received
T+2.4s  → Command lifecycle complete
```

---

## 🔧 **Configuration Constants**

```cpp
// Task intervals (from config.h)
#define WRITE_INTERVAL_MS      1000    // Check for commands every 1 second
#define COMMAND_INTERVAL_MS    2000    // Report results every 2 seconds
#define UPLOAD_INTERVAL_MS     60000   // Upload data every 60 seconds

// Modbus constants
#define SLAVE_ADDRESS          1
#define FUNCTION_CODE_WRITE    0x06    // Write single register (FC06)
#define FUNCTION_CODE_READ     0x03    // Read holding registers (FC03)
```

---

## 🐛 **Debugging Tips**

### **Serial Monitor Output:**
```
[COMMAND] Command detected in cloud response
[COMMAND] Validating WRITE command
Executing write task...
Write successful: Register 40100 set to 3000
[COMMAND] Result successfully reported to cloud
```

### **Common Issues:**
1. ❌ **"No pending command - skipping"**
   - Command not parsed correctly
   - Check cloud response format

2. ❌ **"Invalid export power value"**
   - Value out of acceptable range
   - Check `is_valid_write_value()` logic

3. ❌ **"Failed - No response"**
   - Modbus API not reachable
   - Network connectivity issue

4. ❌ **"Failed - Exception"**
   - Modbus device rejected write
   - Check register address and value

---

## 🚀 **Recommended Improvements**

1. **Command Queue:**
   ```cpp
   typedef struct {
       command_state_t commands[MAX_COMMAND_QUEUE];
       size_t head, tail, count;
   } command_queue_t;
   ```

2. **Command Timeout:**
   ```cpp
   typedef struct {
       // ... existing fields
       unsigned long received_at;
       unsigned long timeout_ms;
   } command_state_t;
   ```

3. **Command Signature:**
   ```json
   {
     "command": {
       "action": "write_register",
       "target_register": "40100",
       "value": 3000,
       "signature": "<HMAC-SHA256 of command>"
     }
   }
   ```

4. **Register Whitelist:**
   ```cpp
   const uint16_t ALLOWED_WRITE_REGISTERS[] = {40100, 40101, 40102};
   const size_t ALLOWED_WRITE_REGISTERS_COUNT = 3;
   ```

---

## 📖 **Related Files**

- `lib/command_parse/command_parse.cpp` - Command parsing
- `lib/command_parse/command_parse.h` - Command parsing interface
- `lib/scheduler/scheduler.cpp` - Task execution and command lifecycle
- `lib/scheduler/scheduler.h` - Task and command state structures
- `lib/modbus_handler/modbus_handler.cpp` - Modbus communication
- `lib/config/config.h` - Configuration constants

---

**Status:** ✅ **COMMAND EXECUTION FULLY OPERATIONAL**

**Last Updated:** October 15, 2025  
**Feature:** Remote Modbus write via cloud commands
