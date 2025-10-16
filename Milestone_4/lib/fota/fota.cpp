#include "fota.h"
#include "config.h"
#include "time_utils.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/pk.h>
#include <mbedtls/base64.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <LittleFS.h>

// Constants
const char *logURL = UPLOAD_API_BASE_URL "/api/fota/log";

// New JSON-based logging system (replaces old buffer-based system)

const char *firmwarePublicKey = R"(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIn8Ze+wsLb6boVAkc90OoCB8/V6o
ri0gie2m8fqXcReMD2T2K0XmbV26lPGiIlathUmiDGxnEsDRBzEOnyL4fw==
-----END PUBLIC KEY-----)";

const char *rootCACertificate = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFBTCCAu2gAwIBAgIQWgDyEtjUtIDzkkFX6imDBTANBgkqhkiG9w0BAQsFADBP
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy
Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa
Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF
bmNyeXB0MQwwCgYDVQQDEwNSMTMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK
AoIBAQClZ3CN0FaBZBUXYc25BtStGZCMJlA3mBZjklTb2cyEBZPs0+wIG6BgUUNI
fSvHSJaetC3ancgnO1ehn6vw1g7UDjDKb5ux0daknTI+WE41b0VYaHEX/D7YXYKg
L7JRbLAaXbhZzjVlyIuhrxA3/+OcXcJJFzT/jCuLjfC8cSyTDB0FxLrHzarJXnzR
yQH3nAP2/Apd9Np75tt2QnDr9E0i2gB3b9bJXxf92nUupVcM9upctuBzpWjPoXTi
dYJ+EJ/B9aLrAek4sQpEzNPCifVJNYIKNLMc6YjCR06CDgo28EdPivEpBHXazeGa
XP9enZiVuppD0EqiFwUBBDDTMrOPAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG
MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/
AgEAMB0GA1UdDgQWBBTnq58PLDOgU9NeT3jIsoQOO9aSMzAfBgNVHSMEGDAWgBR5
tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG
Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD
VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B
AQsFAAOCAgEAUTdYUqEimzW7TbrOypLqCfL7VOwYf/Q79OH5cHLCZeggfQhDconl
k7Kgh8b0vi+/XuWu7CN8n/UPeg1vo3G+taXirrytthQinAHGwc/UdbOygJa9zuBc
VyqoH3CXTXDInT+8a+c3aEVMJ2St+pSn4ed+WkDp8ijsijvEyFwE47hulW0Ltzjg
9fOV5Pmrg/zxWbRuL+k0DBDHEJennCsAen7c35Pmx7jpmJ/HtgRhcnz0yjSBvyIw
6L1QIupkCv2SBODT/xDD3gfQQyKv6roV4G2EhfEyAsWpmojxjCUCGiyg97FvDtm/
NK2LSc9lybKxB73I2+P2G3CaWpvvpAiHCVu30jW8GCxKdfhsXtnIy2imskQqVZ2m
0Pmxobb28Tucr7xBK7CtwvPrb79os7u2XP3O5f9b/H66GNyRrglRXlrYjI1oGYL/
f4I1n/Sgusda6WvA6C190kxjU15Y12mHU4+BxyR9cx2hhGS9fAjMZKJss28qxvz6
Axu4CaDmRNZpK/pQrXF17yXCXkmEWgvSOEZy6Z9pcbLIVEGckV/iVeq0AOo2pkg9
p4QRIy0tK2diRENLSF2KysFwbY6B26BFeFs3v1sYVRhFW9nLkOrQVporCS0KyZmf
wVD89qSTlnctLcZnIavjKsKUu1nA1iU0yYMdYepKR7lWbnwhdx3ewok=
-----END CERTIFICATE-----
)EOF";

bool verifySignature(const String &jsonString, const String &signatureBase64, const char *publicKeyPEM) {
    //Decode Base64 signature
    size_t sigLen;
    uint8_t sigBin[256]; // max ECDSA signature size
    int ret = mbedtls_base64_decode(sigBin, sizeof(sigBin), &sigLen,
                                    (const unsigned char*)signatureBase64.c_str(),
                                    signatureBase64.length());
    if (ret != 0) {
        Serial.println("Base64 decode failed");
        return false;
    }

    //Hash the JSON string (SHA-256)
    uint8_t hash[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts_ret(&sha_ctx, 0); // 0 = SHA-256, not SHA-224
    mbedtls_sha256_update_ret(&sha_ctx, (const unsigned char*)jsonString.c_str(), jsonString.length());
    mbedtls_sha256_finish_ret(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);

    //Parse public key
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)publicKeyPEM, strlen(publicKeyPEM)+1);
    if (ret != 0) {
        Serial.printf("Public key parse error: -0x%04X\n", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    //Verify signature
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sigBin, sigLen);
    mbedtls_pk_free(&pk);

    if (ret == 0) {
        return true; // signature valid
    } else {
        Serial.printf("Signature verification failed: -0x%04X\n", -ret);
        return false;
    }
}

// Extract target version from firmware URL
String extract_target_version(const String& fwUrl) {
    // Simple extraction: look for version pattern in URL
    // Example: ".../firmware-v1.2.0.bin" -> "1.2.0"
    int vIndex = fwUrl.indexOf("-v");
    if (vIndex > 0) {
        int dotIndex = fwUrl.indexOf(".bin", vIndex);
        if (dotIndex > 0) {
            return fwUrl.substring(vIndex + 2, dotIndex);
        }
    }
    return "unknown";
}

// Initialize FOTA log file with START event
bool init_fota_log(const String& jobId, const String& fromVersion, const String& toVersion) {
    // Delete old log if exists
    if (LittleFS.exists("/fota_log.json")) {
        LittleFS.remove("/fota_log.json");
    }
    
    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FOTA] Failed to mount LittleFS");
        return false;
    }
    
    // Create new log file
    File file = LittleFS.open("/fota_log.json", FILE_WRITE);
    if (!file) {
        Serial.println("[FOTA] Failed to create log file");
        return false;
    }
    
    // Create START event JSON
    JsonDocument doc;
    doc["ts"] = get_current_timestamp();
    doc["lvl"] = "INFO";
    doc["msg"] = "FOTA_START";
    doc["job"] = jobId;
    doc["from"] = fromVersion;
    doc["to"] = toVersion;
    
    // Write opening bracket for JSON array
    file.println("[");
    
    // Write START event
    String eventJson;
    serializeJson(doc, eventJson);
    file.println("  " + eventJson);
    
    file.close();
    
    Serial.println("[FOTA] Log initialized with START event");
    return true;
}

// Append error event to log file
bool append_fota_event(const String& level, const String& message, const String& reason = "") {
    if (!LittleFS.begin(true)) {
        Serial.println("[FOTA] Failed to mount LittleFS");
        return false;
    }
    
    File file = LittleFS.open("/fota_log.json", FILE_APPEND);
    if (!file) {
        Serial.println("[FOTA] Failed to open log file for append");
        return false;
    }
    
    // Create event JSON
    JsonDocument doc;
    doc["ts"] = get_current_timestamp();
    doc["lvl"] = level;
    doc["msg"] = message;
    if (reason.length() > 0) {
        doc["reason"] = reason;
    }
    
    // Write event with comma separator
    String eventJson;
    serializeJson(doc, eventJson);
    file.println("  ," + eventJson);
    
    file.close();
    
    Serial.printf("[FOTA] Event logged: %s - %s\n", level.c_str(), message.c_str());
    return true;
}

// Finalize log, upload to cloud, and clean up
bool finalize_and_upload_fota_log(const String& jobId, const String& finalStatus, unsigned long durationMs) {
    if (!LittleFS.begin(true)) {
        Serial.println("[FOTA] Failed to mount LittleFS");
        return false;
    }
    
    // Append closing bracket
    File file = LittleFS.open("/fota_log.json", FILE_APPEND);
    if (file) {
        file.println("]");
        file.close();
    }
    
    // Read complete log file
    file = LittleFS.open("/fota_log.json", FILE_READ);
    if (!file) {
        Serial.println("[FOTA] Failed to open log file for reading");
        return false;
    }
    
    String eventsJson = file.readString();
    file.close();
    
    // Create final payload
    JsonDocument payloadDoc;
    payloadDoc["jobId"] = jobId;
    payloadDoc["final_status"] = finalStatus;
    payloadDoc["duration_ms"] = durationMs;
    
    // Parse events array from file
    JsonDocument eventsDoc;
    DeserializationError error = deserializeJson(eventsDoc, eventsJson);
    if (!error) {
        payloadDoc["events"] = eventsDoc.as<JsonArray>();
    }
    
    // Convert to JSON string
    String payload;
    serializeJson(payloadDoc, payload);
    
    Serial.println("[FOTA] Final log payload:");
    Serial.println(payload);
    
    // Upload to cloud
    HTTPClient http;
    http.begin(logURL);
    http.addHeader("Content-Type", "application/json");
    
    int code = http.POST(payload);
    if (code > 0) {
        Serial.printf("[FOTA] Log upload complete, response: %d\n", code);
    } else {
        Serial.printf("[FOTA] Log upload failed: %s\n", http.errorToString(code).c_str());
    }
    http.end();
    
    // Delete log file after upload
    LittleFS.remove("/fota_log.json");
    Serial.println("[FOTA] Log file deleted");
    
    return (code >= 200 && code < 300);
}

// New function that accepts manifest parameters (for cloud integration)
bool perform_FOTA_with_manifest(int job_id, 
                                const String& fwUrl, 
                                size_t fwSize, 
                                const String& shaExpected, 
                                const String& signature) {
  
  unsigned long startMillis = millis();  // Track start time for duration
  Preferences prefs;
  
  // Check if this is a new update
  prefs.begin("fota", false);
  int stored_job_id = prefs.getInt("job_id", -1);
  size_t stored_offset = prefs.getULong("offset", 0);
  
  if ((stored_job_id >= job_id) && (stored_offset == 0)) {
    Serial.println("[FOTA] No new updates (job_id already processed)");
    prefs.end();
    return false;
  }
  prefs.end();
  
  // Extract version info
  String fromVersion = FIRMWARE_VERSION;
  String toVersion = extract_target_version(fwUrl);
  String jobIdStr = "fota-job-" + String(job_id);
  
  // Initialize log file with START event
  if (!init_fota_log(jobIdStr, fromVersion, toVersion)) {
    Serial.println("[FOTA] Failed to initialize log");
    return false;
  }
  
  Serial.println("[FOTA] Starting firmware download");
  Serial.print("[FOTA] Job ID: "); Serial.println(job_id);
  Serial.print("[FOTA] From: "); Serial.print(fromVersion);
  Serial.print(" → To: "); Serial.println(toVersion);
  Serial.print("[FOTA] Firmware URL: "); Serial.println(fwUrl);
  Serial.print("[FOTA] Firmware Size: "); Serial.print(fwSize); Serial.println(" bytes");
  
  // Create manifest JSON for signature verification
  JsonDocument doc;
  doc["job_id"] = job_id;
  doc["fwUrl"] = fwUrl;
  doc["fwSize"] = fwSize;
  doc["shaExpected"] = shaExpected;
  
  String origManifest;
  serializeJson(doc, origManifest);
  
  // Verify manifest signature
  if (!verifySignature(origManifest, signature, firmwarePublicKey)) {
    Serial.println("[FOTA] Manifest signature invalid");
    append_fota_event("ERROR", "FOTA_FAIL", "SIGNATURE_INVALID");
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }
  
  Serial.println("[FOTA] Manifest signature verified");
  
  // Step 2. Resume download if needed
  prefs.begin("fota", false);
  size_t offset = prefs.getULong("offset", 0);
  if (offset > 0) {
    Serial.print("[FOTA] Resuming download from offset: ");
    Serial.println(offset);
  }
  prefs.putInt("job_id", job_id);
  prefs.end();

  WiFiClientSecure fwClient;
  fwClient.setCACert(rootCACertificate);
  
  HTTPClient fwHttp;
  if (!fwHttp.begin(fwClient, fwUrl)) { 
    Serial.println("[FOTA] Unable to start HTTP client");
    append_fota_event("ERROR", "FOTA_FAIL", "HTTP_CLIENT_FAILED");
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }
  
  char rangeHeader[64];
  sprintf(rangeHeader, "bytes=%u-", (unsigned int)offset);
  fwHttp.addHeader("Range", rangeHeader);
  int respCode = fwHttp.GET();

  if (respCode != HTTP_CODE_PARTIAL_CONTENT && respCode != HTTP_CODE_OK) {
    Serial.printf("[FOTA] HTTP error: %d\n", respCode);
    append_fota_event("ERROR", "FOTA_FAIL", "HTTP_ERROR_" + String(respCode));
    fwHttp.end();
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }

  int totalWritten = offset;
  WiFiClient *stream = fwHttp.getStreamPtr();

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);

  Serial.printf("[FOTA] Running: %s, Next: %s\n", running->label, next->label);

  esp_ota_handle_t otaHandle = 0;
  esp_err_t err = esp_ota_begin(next, fwSize, &otaHandle);
  if (err != ESP_OK) {
    Serial.printf("[FOTA] esp_ota_begin failed: %s\n", esp_err_to_name(err));
    append_fota_event("ERROR", "FOTA_FAIL", "OTA_BEGIN_FAILED");
    fwHttp.end();
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }

  uint8_t *buf = (uint8_t *)malloc(4096);
  if (!buf) {
    Serial.println("[FOTA] Failed to allocate buffer");
    append_fota_event("ERROR", "FOTA_FAIL", "MEMORY_ALLOCATION_FAILED");
    esp_ota_end(otaHandle);
    fwHttp.end();
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }

  unsigned char hashBuf[32];
  size_t bytesRead;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);

  // Download firmware (NO VERBOSE LOGGING)
  while (fwHttp.connected() && (bytesRead = stream->readBytes(buf, 4096)) > 0) {
    err = esp_ota_write(otaHandle, buf, bytesRead);
    if (err != ESP_OK) {
      Serial.printf("[FOTA] esp_ota_write failed: %s\n", esp_err_to_name(err));
      append_fota_event("ERROR", "FOTA_FAIL", "WRITE_FAILED");
      free(buf);
      esp_ota_end(otaHandle);
      fwHttp.end();
      
      unsigned long duration = millis() - startMillis;
      finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
      return false;
    }

    mbedtls_sha256_update_ret(&ctx, buf, bytesRead);
    totalWritten += bytesRead;

    // Persist progress every 100KB (NO LOGGING)
    if (totalWritten % 102400 == 0) {
      prefs.begin("fota", false);
      prefs.putULong("offset", totalWritten);
      prefs.end();
    }

    // Show progress on serial only
    Serial.printf("[FOTA] Progress: %d/%d bytes (%.2f%%)\r\n", 
                  totalWritten, (int)fwSize, 100.0 * totalWritten / fwSize);
  }

  Serial.println("[FOTA] Download complete");
  prefs.begin("fota", false);
  prefs.putULong("offset", totalWritten);
  prefs.end();
  
  free(buf);
  buf = nullptr;

  mbedtls_sha256_finish_ret(&ctx, hashBuf);
  mbedtls_sha256_free(&ctx);
  fwHttp.end();

  String computedHash;
  char hexBuf[3];
  for (size_t i = 0; i < sizeof(hashBuf); i++) {
    sprintf(hexBuf, "%02X", hashBuf[i]);
    computedHash += hexBuf;
  }
  
  // Verify hash
  if (!computedHash.equalsIgnoreCase(shaExpected)) {   
    Serial.println("[FOTA] SHA mismatch");
    Serial.println("[FOTA] Computed: " + computedHash);
    Serial.println("[FOTA] Expected: " + shaExpected);
    append_fota_event("ERROR", "FOTA_FAIL", "HASH_MISMATCH");
    esp_ota_abort(otaHandle);
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }
  
  Serial.println("[FOTA] SHA verified");

  err = esp_ota_end(otaHandle);
  if (err != ESP_OK) {
    Serial.printf("[FOTA] esp_ota_end failed: %s\n", esp_err_to_name(err));
    append_fota_event("ERROR", "FOTA_FAIL", "OTA_END_FAILED");
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }

  err = esp_ota_set_boot_partition(next);
  if (err != ESP_OK) {
    Serial.printf("[FOTA] esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
    append_fota_event("ERROR", "FOTA_FAIL", "SET_BOOT_PARTITION_FAILED");
    
    unsigned long duration = millis() - startMillis;
    finalize_and_upload_fota_log(jobIdStr, "FAILURE", duration);
    return false;
  }

  prefs.begin("fota", false);
  prefs.putULong("offset", 0);
  prefs.end();

  // SUCCESS - Log final event
  Serial.println("[FOTA] Firmware validated and ready");
  append_fota_event("INFO", "FOTA_SUCCESS");
  
  unsigned long duration = millis() - startMillis;
  finalize_and_upload_fota_log(jobIdStr, "SUCCESS", duration);
  
  return true;
}