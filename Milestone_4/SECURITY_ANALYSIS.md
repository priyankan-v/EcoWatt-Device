# Security Layer Analysis - Data Upload Implementation

**Analysis Date:** October 15, 2025  
**Firmware Version:** 1.0.0  
**Security Assessment:** ⚠️ **PARTIALLY IMPLEMENTED** (Mock Security)

---

## Executive Summary

The security layer for data uploads is **partially implemented** with placeholders for actual encryption. The current implementation provides:

✅ **Authentication** - API Key-based  
✅ **Integrity** - HMAC-SHA256 MAC verification  
✅ **Replay Protection** - Nonce-based  
✅ **Data Encoding** - Base64 encoding  
✅ **Transport Security** - HTTPS (TLS/SSL)  
❌ **Encryption** - NOT IMPLEMENTED (marked as "mock encrypted")  

**Current Status:** Security framework is in place, but actual payload encryption is disabled/mocked.

---

## Detailed Security Implementation

### 1. Authentication Layer

#### API Key Authentication
**Location:** `config.h` (line 18), `api_client.cpp` (lines 92-93)

```cpp
// Configuration
#define UPLOAD_API_KEY "ColdPlay2025"

// Implementation
http.addHeader(F("Authorization"), api_key);
```

**Analysis:**
- ✅ Simple bearer token authentication
- ✅ Sent in HTTP Authorization header
- ⚠️ Static key (no rotation mechanism)
- ⚠️ Hardcoded in firmware (visible if extracted)
- ✅ Protected by HTTPS in transit

**Security Level:** **BASIC** - Suitable for device-to-cloud authentication but vulnerable if firmware is extracted.

---

### 2. Message Authentication (MAC)

#### HMAC-SHA256 Implementation
**Location:** `encryptionAndSecurity.cpp` (lines 62-88)

```cpp
String generateMAC(const uint8_t* payload, size_t length) {
    uint8_t mac[32]; // SHA-256 outputs a 32-byte hash
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    // Perform the HMAC operation using Pre-Shared Key (PSK)
    mbedtls_md_hmac(
        md_info,
        (const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK),
        payload, length,
        mac
    );

    // Convert to hexadecimal string
    String macHex = "";
    macHex.reserve(64);
    for (size_t i = 0; i < sizeof(mac); i++) {
        if (mac[i] < 0x10) {
            macHex += "0";
        }
        macHex += String(mac[i], HEX);
    }
    return macHex;
}
```

**Usage in Upload Flow:** `scheduler.cpp` (line 418)
```cpp
// Generate MAC for the Base64-encoded payload
String mac = generateMAC(upload_frame_base64);
```

**Analysis:**
- ✅ Strong cryptographic hash (SHA-256)
- ✅ HMAC provides message integrity AND authentication
- ✅ Prevents tampering with uploaded data
- ✅ Uses mbedtls (industry-standard library)
- ⚠️ PSK is hardcoded: `"ColdPlay@EcoWatt2025"`
- ⚠️ MAC computed on Base64 string (not raw bytes - adds overhead)

**PSK Configuration:** `config.h` (line 22)
```cpp
#define UPLOAD_PSK "ColdPlay@EcoWatt2025"
```

**Security Level:** **GOOD** - Provides strong integrity protection, but PSK should be device-specific.

**How it Works:**
1. Device encodes upload frame to Base64
2. Computes HMAC-SHA256(PSK, Base64_payload)
3. Sends MAC in HTTP header
4. Server verifies: recomputes MAC and compares
5. If MAC matches → payload is authentic and unmodified
6. If MAC differs → reject (tampering detected)

---

### 3. Replay Protection

#### Nonce Management
**Location:** `encryptionAndSecurity.cpp` (lines 92-110)

```cpp
class NonceManager {
public:
    void begin();
    uint32_t getAndIncrementNonce();
};

void NonceManager::begin() {
    EEPROM.begin(4);  // 4 bytes for uint32_t nonce
}

uint32_t NonceManager::getAndIncrementNonce() {
    uint32_t currentNonce;
    EEPROM.get(NONCE_ADDRESS, currentNonce);
    uint32_t nextNonce = currentNonce + 1;
    EEPROM.put(NONCE_ADDRESS, nextNonce);
    EEPROM.commit();
    return currentNonce;
}
```

**Usage:** `scheduler.cpp` (lines 407-410)
```cpp
// Get a unique nonce for this transaction
uint32_t nonce = nonceManager.getAndIncrementNonce();
Serial.print(F("[SECURITY] Using Nonce: "));
Serial.println(nonce);
```

**Initialization:** `main.cpp` (line 17)
```cpp
NonceManager nonceManager;

void setup() {
    nonceManager.begin();
}
```

**Analysis:**
- ✅ Persistent counter in EEPROM (survives reboots)
- ✅ Monotonically increasing (never repeats)
- ✅ Prevents replay attacks (server can reject old nonces)
- ✅ Simple and effective
- ⚠️ EEPROM has limited write cycles (~100k writes)
- ⚠️ No nonce synchronization mechanism if server resets
- ⚠️ uint32_t limit: ~4.3 billion uploads before overflow

**EEPROM Address:** `config.h` (line 23)
```cpp
#define NONCE_ADDRESS 0  // EEPROM address for storing nonce
```

**Security Level:** **GOOD** - Effective replay protection with minor caveats.

**How it Works:**
1. Device retrieves current nonce from EEPROM
2. Sends nonce in HTTP header
3. Increments and saves next nonce to EEPROM
4. Server tracks last valid nonce per device
5. Server rejects requests with nonce ≤ last_seen_nonce
6. Prevents attacker from resending captured packets

---

### 4. Data Encoding

#### Base64 Encoding
**Location:** `encryptionAndSecurity.cpp` (lines 10-30)

```cpp
String encodeBase64(const uint8_t* payload, size_t length) {
    size_t encodedLen = 0;
    // First call to get the required length
    mbedtls_base64_encode(NULL, 0, &encodedLen, payload, length);

    char encodedPayload[encodedLen + 1]; // +1 for null terminator

    // Second call to perform the encoding
    mbedtls_base64_encode((unsigned char*)encodedPayload, encodedLen, 
                         &encodedLen, payload, length);
    
    encodedPayload[encodedLen] = '\0';
    return String(encodedPayload);
}
```

**Usage:** `scheduler.cpp` (line 413)
```cpp
// Encode payload to Base64 directly from byte array
String upload_frame_base64 = encodeBase64(upload_frame_with_crc, compressed_data_len + 3);
```

**Analysis:**
- ✅ Converts binary data to ASCII-safe format
- ✅ Compatible with HTTP headers and JSON
- ✅ Uses mbedtls (reliable implementation)
- ⚠️ NOT encryption (just encoding)
- ⚠️ Increases size by ~33% (Base64 overhead)
- ⚠️ Easily reversible (anyone can decode)

**Security Level:** **NONE** - This is encoding, not encryption. Provides no confidentiality.

**Purpose:** Makes binary data HTTP-friendly, NOT security.

---

### 5. Transport Security

#### HTTPS (TLS/SSL)
**Location:** `config.h` (line 17), `api_client.cpp` (line 89)

```cpp
// Configuration - Note the HTTPS URL
#define UPLOAD_API_BASE_URL "https://eco-watt-cloud.vercel.app"

// Usage
http.begin(url);  // HTTPClient automatically handles HTTPS
```

**Analysis:**
- ✅ Industry-standard transport encryption
- ✅ Protects data in transit from eavesdropping
- ✅ Server certificate validation (if enabled)
- ✅ Prevents man-in-the-middle attacks
- ⚠️ Certificate pinning not implemented
- ⚠️ Relies on ESP32's root CA store

**Security Level:** **STRONG** - TLS 1.2/1.3 provides excellent transport security.

---

### 6. Payload Encryption (CRITICAL ISSUE)

#### Mock Encryption Flag
**Location:** `api_client.cpp` (line 94)

```cpp
http.addHeader(F("payload_is_mock_encrypted"), "true");
```

**What This Means:**
- ⚠️ **Payload is NOT actually encrypted**
- ⚠️ Header tells server "treat this as if it's encrypted, but it's not"
- ⚠️ Placeholder for future encryption implementation
- ⚠️ Data is sent as Base64-encoded plaintext

**Commented Code in scheduler.cpp (lines 370-387):**
```cpp
/*
// Encryption step removed for now
uint8_t encrypted_frame[encrypted_frame_len];
memset(encrypted_frame, 0, encrypted_frame_len);
encrypt_compressed_frame(compressed_data_frame, compressed_data_len + 1, encrypted_frame); 

// Add MAC 
uint8_t encrypted_frame_with_mac[encrypted_frame_len + 8];
memcpy(encrypted_frame_with_mac, encrypted_frame, encrypted_frame_len);
calculate_and_add_mac(encrypted_frame, encrypted_frame_len, 
                     encrypted_frame_with_mac + encrypted_frame_len);
*/
```

**Analysis:**
- ❌ **No actual encryption implemented**
- ❌ Data payload visible to anyone who captures traffic (despite HTTPS)
- ❌ Server-side can see plaintext data
- ❌ Violates principle of defense in depth
- ✅ Framework exists for future implementation
- ✅ Server is aware it's mock encrypted

**Security Level:** **NONE** - This is the biggest security gap.

**Risk Assessment:**
- **Low risk** if server infrastructure is trusted
- **Medium risk** if server is compromised
- **High risk** if regulatory compliance requires end-to-end encryption

---

## Complete Upload Flow with Security

### Step-by-Step Breakdown

**Location:** `scheduler.cpp` `execute_upload_task()` (lines 380-422)

```cpp
// STEP 1: Add CRC for frame integrity
uint8_t upload_frame_with_crc[compressed_data_len + 3];
append_crc_to_upload_frame(compressed_data_frame, compressed_data_len + 1, 
                           upload_frame_with_crc);

// STEP 2: Get monotonically increasing nonce (replay protection)
uint32_t nonce = nonceManager.getAndIncrementNonce();
Serial.print(F("[SECURITY] Using Nonce: "));
Serial.println(nonce);

// STEP 3: Encode payload to Base64 (HTTP-friendly format)
String upload_frame_base64 = encodeBase64(upload_frame_with_crc, 
                                          compressed_data_len + 3);

// STEP 4: Generate HMAC-SHA256 MAC (integrity + authentication)
String mac = generateMAC(upload_frame_base64);
Serial.print(F("[SECURITY] Generated MAC: "));
Serial.println(mac);

// STEP 5: Send to cloud with security headers
String response = upload_api_send_request_with_retry(
    url, method, api_key,           // Authentication
    upload_frame_with_crc,          // Payload (binary)
    compressed_data_len + 3,        // Payload size
    String(nonce),                  // Replay protection
    mac                             // Integrity verification
);
```

### HTTP Request Structure

```http
POST /api/cloud/write HTTP/1.1
Host: eco-watt-cloud.vercel.app
Content-Type: application/octet-stream
Authorization: ColdPlay2025
payload_is_mock_encrypted: true
nonce: 12345
mac: a1b2c3d4e5f6...32-byte-hex...

[Binary payload: metadata + compressed_data + CRC (2 bytes)]
```

### Security Headers Breakdown

| Header | Purpose | Value Example | Security Function |
|--------|---------|---------------|-------------------|
| `Authorization` | Authentication | `ColdPlay2025` | Identifies legitimate device |
| `payload_is_mock_encrypted` | Encryption flag | `true` | Server knows it's NOT encrypted |
| `nonce` | Replay protection | `12345` | Prevents replay attacks |
| `mac` | Integrity | `a1b2c3...` (64 hex chars) | Verifies payload not tampered |

---

## Security Strengths

### ✅ What's Working Well

1. **Strong Integrity Protection**
   - HMAC-SHA256 prevents tampering
   - CRC catches transmission errors
   - MAC validates entire payload

2. **Replay Attack Prevention**
   - Persistent nonce in EEPROM
   - Monotonically increasing counter
   - Server can reject old requests

3. **Transport Security**
   - HTTPS encrypts data in transit
   - Prevents network eavesdropping
   - Standard TLS 1.2/1.3

4. **Authentication**
   - API key validates device identity
   - Server can authorize/deny access
   - Simple but effective

5. **Modular Design**
   - Encryption functions stubbed out
   - Easy to add real encryption later
   - Clean separation of concerns

---

## Security Weaknesses

### ❌ Critical Issues

1. **No Payload Encryption**
   - **Risk:** Data visible to server (and if server compromised)
   - **Impact:** Confidentiality completely absent
   - **Mitigation:** Implement AES-128/256 encryption
   - **Status:** `payload_is_mock_encrypted: true` flag present

2. **Hardcoded Secrets**
   - **API Key:** `"ColdPlay2025"` hardcoded in firmware
   - **PSK:** `"ColdPlay@EcoWatt2025"` hardcoded
   - **Risk:** Secrets extractable from firmware dump
   - **Mitigation:** Use secure element (ATECC608) or provisioning

3. **Static API Key**
   - **Risk:** If one device compromised, all devices use same key
   - **Mitigation:** Device-specific keys or certificate-based auth

4. **No Certificate Pinning**
   - **Risk:** Man-in-the-middle if attacker has CA cert
   - **Mitigation:** Pin server certificate or public key

### ⚠️ Medium Issues

5. **EEPROM Wear**
   - **Risk:** Nonce storage wears out EEPROM (~100k writes)
   - **Calculation:** At 4 uploads/hour = ~2.8 years until wear
   - **Mitigation:** Use NVS (wear-leveling) or external flash

6. **MAC Computed on Base64**
   - **Inefficiency:** Computing MAC on encoded data (33% larger)
   - **Impact:** Slower, more memory
   - **Mitigation:** Compute MAC on raw bytes, then encode

7. **No Key Rotation**
   - **Risk:** Long-term key compromise accumulates risk
   - **Mitigation:** Periodic PSK rotation mechanism

8. **Nonce Overflow**
   - **Risk:** uint32_t wraps after 4.3 billion uploads
   - **Calculation:** At 4 uploads/hour = ~122,000 years (unlikely)
   - **Impact:** Low priority

---

## Threat Model Analysis

### Threats Mitigated ✅

1. **Tampering (Integrity Attacks)**
   - Attacker modifies data in transit
   - **Mitigation:** HMAC-SHA256 MAC
   - **Effectiveness:** STRONG

2. **Replay Attacks**
   - Attacker resends captured valid request
   - **Mitigation:** Monotonic nonce
   - **Effectiveness:** GOOD (until nonce sync issue)

3. **Network Eavesdropping**
   - Attacker sniffs WiFi/network traffic
   - **Mitigation:** HTTPS (TLS)
   - **Effectiveness:** STRONG (for transport layer)

4. **Unauthorized Access**
   - Rogue device attempts upload
   - **Mitigation:** API key authentication
   - **Effectiveness:** BASIC (all devices share key)

### Threats NOT Mitigated ❌

5. **Data Confidentiality (Payload)**
   - Attacker with server access sees data
   - **Current State:** NO PROTECTION
   - **Required:** AES encryption

6. **Firmware Extraction**
   - Attacker dumps firmware, extracts secrets
   - **Current State:** Keys readable in flash
   - **Required:** Secure element or encrypted storage

7. **Server Compromise**
   - Malicious server operator or hack
   - **Current State:** Server sees plaintext data
   - **Required:** End-to-end encryption

8. **Man-in-the-Middle (MITM)**
   - Attacker with CA certificate
   - **Current State:** Relies on ESP32 root CA store
   - **Required:** Certificate pinning

---

## Recommendations

### Priority 1: Critical (Security Gaps)

1. **Implement Actual Payload Encryption**
   ```cpp
   // Replace mock encryption with real AES-128/256
   uint8_t encrypted_payload[payload_len + 16]; // Add IV
   aes_encrypt_cbc(payload, payload_len, key, iv, encrypted_payload);
   ```
   - **Algorithm:** AES-256-CBC or AES-128-GCM
   - **Library:** mbedtls (already included)
   - **Key:** Device-specific or session key

2. **Device-Specific Keys**
   - Generate unique API key per device during provisioning
   - Store in secure element (ATECC608) or encrypted NVS
   - Backend tracks device_id → api_key mapping

3. **Remove Hardcoded Secrets**
   - Use secure provisioning process
   - Store keys in encrypted partition
   - Or use hardware security module

### Priority 2: Important (Defense in Depth)

4. **Certificate Pinning**
   ```cpp
   // Pin server certificate or public key
   client.setCACert(server_cert);
   client.verify(true);
   ```

5. **Migrate Nonce to NVS**
   ```cpp
   // Use ESP32 NVS (wear-leveling built-in)
   nvs_handle_t nonce_handle;
   nvs_open("security", NVS_READWRITE, &nonce_handle);
   nvs_get_u32(nonce_handle, "nonce", &nonce);
   ```

6. **Add Timestamp to Requests**
   ```cpp
   // Reject requests older than X minutes
   uint32_t timestamp = get_current_timestamp();
   // Send in header or payload
   ```

### Priority 3: Enhancement

7. **Implement Key Rotation**
   - Periodic PSK change via secure channel
   - Rollover mechanism for smooth transition

8. **Add Request Signing**
   - Sign entire HTTP request (not just payload)
   - Prevents header tampering

9. **Rate Limiting**
   - Backend tracks upload frequency per device
   - Reject excessive requests (DoS protection)

---

## Comparison: Current vs. Ideal Implementation

| Security Feature | Current Status | Ideal Implementation |
|------------------|----------------|----------------------|
| **Authentication** | ✅ Static API key | 🎯 Device-specific certificates |
| **Integrity** | ✅ HMAC-SHA256 | ✅ HMAC-SHA256 (good) |
| **Replay Protection** | ✅ Nonce (EEPROM) | 🎯 Nonce (NVS) + Timestamp |
| **Encoding** | ✅ Base64 | ✅ Base64 (acceptable) |
| **Transport Security** | ✅ HTTPS | 🎯 HTTPS + Cert Pinning |
| **Payload Encryption** | ❌ Mock only | 🎯 AES-256-GCM |
| **Key Storage** | ❌ Hardcoded | 🎯 Secure element (ATECC608) |
| **Key Management** | ❌ Static | 🎯 Rotation every 30-90 days |

---

## Code Examples for Improvements

### 1. Add Real AES Encryption

```cpp
// In encryptionAndSecurity.cpp
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

bool encryptPayload(const uint8_t* plaintext, size_t plaintext_len,
                   uint8_t* ciphertext, size_t* ciphertext_len,
                   const uint8_t* key, const uint8_t* iv) {
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    // Setup AES-256-GCM
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) return false;
    
    // Encrypt with authentication
    uint8_t tag[16]; // GCM authentication tag
    ret = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT, plaintext_len,
        iv, 12,  // 96-bit IV
        NULL, 0, // No additional data
        plaintext, ciphertext,
        16, tag
    );
    
    // Append tag to ciphertext
    memcpy(ciphertext + plaintext_len, tag, 16);
    *ciphertext_len = plaintext_len + 16;
    
    mbedtls_gcm_free(&gcm);
    return (ret == 0);
}
```

### 2. Migrate to NVS for Nonce

```cpp
// In encryptionAndSecurity.cpp
#include <nvs_flash.h>
#include <nvs.h>

class NonceManager {
private:
    nvs_handle_t nvs_handle;
public:
    void begin() {
        nvs_flash_init();
        nvs_open("security", NVS_READWRITE, &nvs_handle);
    }
    
    uint32_t getAndIncrementNonce() {
        uint32_t nonce = 0;
        nvs_get_u32(nvs_handle, "nonce", &nonce);
        nonce++;
        nvs_set_u32(nvs_handle, "nonce", nonce);
        nvs_commit(nvs_handle);
        return nonce;
    }
};
```

### 3. Add Certificate Pinning

```cpp
// In api_client.cpp
const char* server_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKL...
[Full server certificate here]
-----END CERTIFICATE-----
)EOF";

// In upload_api_send_request
WiFiClientSecure client;
client.setCACert(server_cert);
client.setInsecure(false); // Enforce certificate validation
```

---

## Conclusion

### Current Security Posture: **MODERATE**

**Strengths:**
- ✅ Strong integrity protection (HMAC-SHA256)
- ✅ Replay attack prevention (nonce)
- ✅ Transport security (HTTPS)
- ✅ Framework ready for encryption

**Critical Gap:**
- ❌ **No payload encryption** (mock only)
- ❌ **Hardcoded secrets** in firmware

**Overall Assessment:**
The security implementation provides a **solid foundation** for authentication, integrity, and replay protection. However, the **lack of actual payload encryption** is a significant gap. The `payload_is_mock_encrypted: true` flag indicates awareness of this limitation.

**Recommended Action:**
1. **Immediate:** Document the mock encryption status clearly
2. **Short-term:** Implement AES-256-GCM encryption (1-2 sprints)
3. **Medium-term:** Migrate to device-specific keys and NVS storage
4. **Long-term:** Add hardware security module (ATECC608)

**Is it "correctly implemented"?**
- ✅ The implemented features (MAC, nonce, Base64) are correctly coded
- ⚠️ But the overall security is **incomplete** due to missing encryption
- ✅ The architecture is sound and ready for encryption addition

**For production use:** The current implementation is acceptable for **non-sensitive data** or **trusted server environments**. For sensitive data or regulatory compliance, **encryption must be added**.

---

**Document Version:** 1.0  
**Analysis Date:** October 15, 2025  
**Reviewed By:** AI Security Analysis  
**Next Review:** After encryption implementation
