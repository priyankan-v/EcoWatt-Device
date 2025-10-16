# 📋 Cloud Upload Response Format - Complete Specification

## 🎯 **What Your Cloud Should Reply to Upload Request**

Your cloud server's response to `/api/cloud/upload` can include **multiple optional features**. Here's the exact format the device expects:

---

## ✅ **MINIMUM REQUIRED RESPONSE**

### **Simple Success (No Additional Features):**

```json
{
  "status": "success"
}
```

**What Happens:**
- ✅ Upload acknowledged
- ✅ Buffer cleared
- ❌ No command sent
- ❌ No config update
- ❌ No FOTA

**Code Validation:**
```cpp
// From cloudAPI_handler.cpp, line 16-18
if (response.indexOf(F("\"status\"")) >= 0 && 
    (response.indexOf(F("\"success\"")) >= 0 || response.indexOf(F("success")) >= 0)) {
    // Upload validated successfully
}
```

---

## 📦 **FULL RESPONSE FORMAT (All Features)**

### **Complete Response with All Optional Fields:**

```json
{
  "status": "success",
  "message": "Data uploaded successfully",
  "timestamp": "2025-10-15T14:30:45Z",
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 3000
  },
  "config_update": {
    "upload_interval_ms": 20000,
    "sampling_interval_ms": 5000,
    "slave_address": 17,
    "register_count": 10
  },
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://your-server.com/firmware/v1.2.0.bin",
    "fwSize": 1048576,
    "shaExpected": "abc123def456...",
    "signature": "signature_here"
  }
}
```

---

## 🔍 **DETAILED BREAKDOWN OF EACH SECTION**

### **1️⃣ Status Section (REQUIRED)**

```json
{
  "status": "success"
}
```

**Requirements:**
- ✅ Must contain `"status"` key
- ✅ Value must contain `"success"` (case-insensitive)
- ✅ Can also be `"status": "success"` or `"status":"success"` (whitespace flexible)

**Alternative (Error Response):**
```json
{
  "status": "error",
  "error": "Invalid payload format"
}
```

**Code:**
```cpp
// Validation logic (cloudAPI_handler.cpp)
if (response.indexOf(F("\"status\"")) >= 0 && 
    (response.indexOf(F("\"success\"")) >= 0 || response.indexOf(F("success")) >= 0)) {
    return true;  // Valid
}

if (response.indexOf(F("\"error\"")) >= 0) {
    // Extract and log error message
    return false;
}
```

---

### **2️⃣ Command Section (OPTIONAL)**

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

#### **Field Specifications:**

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `action` | string | ✅ YES | Command type | `"write_register"` or `"read_register"` |
| `target_register` | string | ✅ YES | Modbus register address | `"40100"` |
| `value` | number | ⚠️ Only for write | Value to write | `3000` |

#### **Supported Actions:**

**1. Write Register:**
```json
{
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 3000
  }
}
```

**2. Read Register (parsed but not fully implemented):**
```json
{
  "command": {
    "action": "read_register",
    "target_register": "40100"
  }
}
```

#### **Important Notes:**
- ⚠️ `target_register` is a **STRING**, not a number
- ⚠️ `value` is a **NUMBER**, not a string
- ✅ Action is case-insensitive (`"WRITE_REGISTER"` works too)
- ❌ Only `write_register` and `read_register` are supported

#### **Code Parsing:**
```cpp
// From command_parse.cpp, line 15-60
int cmd_index = response.indexOf(F("\"command\""));
if (cmd_index >= 0) {
    // Extract action
    action = substring between "action":"..." and next "
    
    // Extract target_register
    target_register = substring between "target_register":"..." and next "
    
    // Extract value (only for write_register)
    if (action == "write_register") {
        value = number after "value":
    }
}
```

#### **What Happens:**
```cpp
// From scheduler.cpp, line 445-458
if (extract_command(response, action, reg, val)) {
    if (action.equalsIgnoreCase("write_register")) {
        // Store command
        current_command.pending = true;
        current_command.register_address = reg;
        current_command.value = val;
        
        // Enable execution tasks
        tasks[TASK_WRITE_REGISTER].enabled = true;
        tasks[TASK_COMMAND_HANDLING].enabled = true;
    }
}
```

---

### **3️⃣ Config Update Section (OPTIONAL)**

```json
{
  "status": "success",
  "config_update": {
    "upload_interval_ms": 20000,
    "sampling_interval_ms": 5000,
    "slave_address": 17,
    "register_count": 10
  }
}
```

#### **Field Specifications:**

| Field | Type | Required | Description | Range | Default |
|-------|------|----------|-------------|-------|---------|
| `upload_interval_ms` | number | ❌ NO | Upload frequency | 5000-3600000 | 15000 |
| `sampling_interval_ms` | number | ❌ NO | Modbus polling frequency | 1000-60000 | 3000 |
| `slave_address` | number | ❌ NO | Modbus slave ID | 1-247 | 17 |
| `register_count` | number | ❌ NO | Number of registers to read | 1-10 | 10 |

#### **Important Notes:**
- ✅ All fields are optional (can send partial update)
- ✅ Values validated before applying
- ✅ Device sends ACK back to cloud
- ✅ Changes applied after successful upload

#### **Example Partial Update:**
```json
{
  "status": "success",
  "config_update": {
    "upload_interval_ms": 30000
  }
}
```
**Only upload interval changes, others remain unchanged**

#### **Code Parsing:**
```cpp
// From cloudAPI_handler.cpp, line 40-72
JsonDocument doc;
deserializeJson(doc, response);

if (doc["config_update"].is<JsonObject>()) {
    // Extract entire config_update object
    // Validate and apply changes
    // Generate ACK to send back to cloud
}
```

#### **What Happens:**
```cpp
// From scheduler.cpp, line 476-491
String config_ack = config_process_cloud_response(response);
if (config_ack.length() > 0) {
    send_config_ack_to_cloud(config_ack);  // Send to /api/config_ack
}

if (config_has_pending_changes()) {
    config_apply_pending_changes();  // Apply validated changes
}
```

#### **Config ACK Format (Device sends back):**
```json
{
  "config_ack": {
    "status": "applied",
    "timestamp": "2025-10-15T14:30:45Z",
    "updated_fields": ["upload_interval_ms"]
  }
}
```

---

### **4️⃣ FOTA Section (OPTIONAL)**

```json
{
  "status": "success",
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://your-server.com/firmware/v1.2.0.bin",
    "fwSize": 1048576,
    "shaExpected": "abc123def456789...",
    "signature": "optional_signature_here"
  }
}
```

#### **Field Specifications:**

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `job_id` | number | ✅ YES | FOTA job identifier | `12345` |
| `fwUrl` | string | ✅ YES | Firmware download URL | `"https://..."` |
| `fwSize` | number | ✅ YES | Firmware size in bytes | `1048576` |
| `shaExpected` | string | ✅ YES | SHA-256 hash of firmware | `"abc123..."` |
| `signature` | string | ✅ YES | Firmware signature | `"sig..."` |

#### **Important Notes:**
- ✅ All 5 fields are required if FOTA section present
- ⚠️ Firmware URL must be HTTPS (HTTP may work but not secure)
- ⚠️ FOTA execution **BLOCKS all other tasks** (45-180 seconds)
- ⚠️ Device will **restart automatically** after successful FOTA

#### **Code Parsing:**
```cpp
// From cloudAPI_handler.cpp, line 99-143
JsonDocument doc;
deserializeJson(doc, response);

if (doc["fota"].is<JsonObject>()) {
    JsonObject fota = doc["fota"];
    
    // All 5 fields must be present
    if (fota.containsKey("job_id") && 
        fota.containsKey("fwUrl") && 
        fota.containsKey("fwSize") && 
        fota.containsKey("shaExpected") && 
        fota.containsKey("signature")) {
        
        // Extract and initiate FOTA
    }
}
```

#### **What Happens:**
```cpp
// From scheduler.cpp, line 493-515
if (parse_fota_manifest_from_response(response, job_id, fwUrl, fwSize, shaExpected, signature)) {
    Serial.println(F("[FOTA] Firmware update available"));
    
    bool fota_success = perform_FOTA_with_manifest(job_id, fwUrl, fwSize, shaExpected, signature);
    
    if (fota_success) {
        delay(2000);
        ESP.restart();  // ← DEVICE RESTARTS!
    }
}
```

---

## 📊 **RESPONSE PROCESSING ORDER**

The device processes the response in this order:

```
1. Validate Response
   ├── Check "status": "success"
   └── If invalid → Retry upload
   
2. Extract Command (OPTIONAL)
   ├── Parse "command" section
   ├── Store command
   └── Enable TASK_WRITE_REGISTER
   
3. Process Config Update (OPTIONAL)
   ├── Parse "config_update" section
   ├── Validate changes
   ├── Send ACK to /api/config_ack
   └── Apply changes
   
4. Check for FOTA (OPTIONAL)
   ├── Parse "fota" section
   ├── Download firmware
   ├── Verify SHA-256
   └── Flash and restart
   
5. Clear Buffer
   └── Free memory for next cycle
```

---

## 🧪 **EXAMPLE RESPONSES**

### **Example 1: Simple ACK (No Features)**
```json
{
  "status": "success"
}
```
**Result:** Upload acknowledged, buffer cleared, device continues normally

---

### **Example 2: Command Only**
```json
{
  "status": "success",
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 5000
  }
}
```
**Result:** 
- Upload acknowledged
- Command queued for execution (writes 5000 to register 40100)
- Result sent to `/api/cloud/command_result` after execution

---

### **Example 3: Config Update Only**
```json
{
  "status": "success",
  "config_update": {
    "upload_interval_ms": 30000
  }
}
```
**Result:**
- Upload acknowledged
- Upload interval changed to 30 seconds
- ACK sent to `/api/config_ack`
- Next upload in 30 seconds

---

### **Example 4: Command + Config Update**
```json
{
  "status": "success",
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 3000
  },
  "config_update": {
    "upload_interval_ms": 20000,
    "sampling_interval_ms": 4000
  }
}
```
**Result:**
- Upload acknowledged
- Command queued for execution
- Config changes applied
- ACK sent
- Result sent after command execution

---

### **Example 5: FOTA Only**
```json
{
  "status": "success",
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://updates.example.com/firmware/v1.2.0.bin",
    "fwSize": 1048576,
    "shaExpected": "a3b2c1d4e5f6...",
    "signature": "sign_here"
  }
}
```
**Result:**
- Upload acknowledged
- Firmware download starts (BLOCKS all tasks!)
- Device restarts after successful update
- ⚠️ If FOTA fails, device continues with old firmware

---

### **Example 6: Everything Combined**
```json
{
  "status": "success",
  "message": "Upload successful, command and update queued",
  "timestamp": "2025-10-15T14:30:45Z",
  "command": {
    "action": "write_register",
    "target_register": "40100",
    "value": 3000
  },
  "config_update": {
    "upload_interval_ms": 20000
  },
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://updates.example.com/firmware/v1.2.0.bin",
    "fwSize": 1048576,
    "shaExpected": "a3b2c1d4e5f6...",
    "signature": "sign_here"
  }
}
```
**Processing Order:**
1. ✅ Validate response
2. ✅ Parse command (queued)
3. ✅ Parse config (applied + ACK sent)
4. ✅ Parse FOTA (downloads, flashes, **RESTARTS DEVICE**)
5. ❌ Command never executes (device restarted before execution)

**⚠️ WARNING:** If FOTA is present, the device will restart, so commands may not execute!

---

## ❌ **INVALID RESPONSES**

### **Example 1: Missing Status**
```json
{
  "message": "Upload successful"
}
```
**Result:** ❌ Validation fails, upload retried

---

### **Example 2: Wrong Status Value**
```json
{
  "status": "ok"
}
```
**Result:** ❌ Validation fails (must contain "success")

---

### **Example 3: Invalid Command Action**
```json
{
  "status": "success",
  "command": {
    "action": "delete_register",  ← Invalid
    "target_register": "40100"
  }
}
```
**Result:** ⚠️ Command ignored, upload still successful

---

### **Example 4: Missing FOTA Fields**
```json
{
  "status": "success",
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://updates.example.com/firmware.bin"
    // Missing: fwSize, shaExpected, signature
  }
}
```
**Result:** ⚠️ FOTA ignored, upload still successful

---

## 🔐 **SECURITY CONSIDERATIONS**

### **What the Device Sends (Encrypted):**
```
Headers:
- Content-Type: application/octet-stream
- encryption: aes-256-cbc
- nonce: <32-bit unsigned int>
- mac: <HMAC-SHA256 Base64>

Body:
<AES-256-CBC encrypted payload (Base64)>
```

### **What the Cloud Should Verify:**
1. ✅ Verify HMAC-SHA256 signature
2. ✅ Check nonce is incrementing (replay protection)
3. ✅ Decrypt payload using shared PSK
4. ✅ Validate decrypted data format

### **What the Cloud Should NOT Send:**
- ❌ Sensitive data in plaintext (response is NOT encrypted)
- ❌ Commands without authentication
- ❌ FOTA URLs without HTTPS
- ❌ Firmware without signature verification

---

## 📝 **QUICK REFERENCE**

### **Minimum Response:**
```json
{"status": "success"}
```

### **Command Response:**
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

### **Config Response:**
```json
{
  "status": "success",
  "config_update": {
    "upload_interval_ms": 20000
  }
}
```

### **FOTA Response:**
```json
{
  "status": "success",
  "fota": {
    "job_id": 12345,
    "fwUrl": "https://server.com/fw.bin",
    "fwSize": 1048576,
    "shaExpected": "abc...",
    "signature": "sig..."
  }
}
```

---

**Document Version:** 1.0  
**Last Updated:** October 15, 2025  
**Status:** ✅ **COMPLETE AND TESTED**
