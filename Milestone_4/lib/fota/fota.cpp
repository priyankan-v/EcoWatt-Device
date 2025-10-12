#include "fota.h"
#include "config.h"
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
#include "time.h"

// Constants
const char *manifestURL = UPLOAD_API_BASE_URL "/api/fota/manifest";
const char *logURL = UPLOAD_API_BASE_URL "/api/fota/log";
const char* ntpServer = NTP_SERVER;

String logBuffer[20];
int logCount = 0;

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

void logMessage(const String &level, const String &msg) {
  time_t t = time(NULL);
  struct tm* tm_info = localtime(&t);

  char buf[50];
  sprintf(buf, "[%04d-%02d-%02d %02d:%02d:%02d]",
          tm_info->tm_year + 1900,
          tm_info->tm_mon + 1,
          tm_info->tm_mday,
          tm_info->tm_hour,
          tm_info->tm_min,
          tm_info->tm_sec);

  String timestamp = String(buf);
  String logMsg = timestamp + " " + level + " - " + msg;
  logBuffer[logCount++] = logMsg;
  Serial.println(logMsg);
}

void store_and_upload_log(){ 
  logMessage("Info", "Uploading logs");

  if (!LittleFS.begin(true)) {
  logMessage("Error", "LittleFS mount failed");
  }

  File file = LittleFS.open("/log.txt", FILE_WRITE); // set to FILE_APPEND to keep old logs
  for (int i = 0; i < logCount; i++) {
    file.println(logBuffer[i]);
  }

  file.flush();
  file.close();
  logCount = 0; 

  file = LittleFS.open("/log.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open log.txt for reading");
    return;
  }
  String content = file.readString();
  file.close();

  HTTPClient http;
  http.begin(logURL);
  http.addHeader("Content-Type", "text/plain");
  int code = http.POST(content);
  if (code > 0) {
    Serial.printf("Upload complete, response: %d\n", code);
  } else {
    Serial.printf("Upload failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
};

bool perform_FOTA() {
  Preferences prefs;
  WiFiClientSecure client;
  HTTPClient http;
  client.setCACert(rootCACertificate); // Replace the certificate in case it expires
  // client.setInsecure();             // Use only for preproduction testing

  // Step 1. Download manifest
  // Remove client when CA certificate verification is not needed
  if (!http.begin(client, manifestURL)) { 
    logMessage("Error", String("Unable to start http client on: ") + manifestURL);
    return 0;
  }; 

  Serial.println("Root CA Certificate verified");
  int code = http.GET();
  if (code != 200) {
    logMessage("Error", "http GET failed for manifest with code:" + String(code));
    return 0;
  };

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  deserializeJson(doc, payload);
  int job_id = doc["job_id"];
  String fwUrl = doc["fwUrl"];
  size_t fwSize = doc["fwSize"]; //no of bytes
  String shaExpected = doc["shaExpected"];
  String signature = doc["signature"].as<String>();

  prefs.begin("fota", false);
  if ((prefs.getInt("job_id", -1) >= job_id) && (prefs.getULong("offset", 0) == 0)) {
    Serial.println("No new updates");
    return 0;
  };
  prefs.end();

  Serial.println("Job ID: " + job_id);
  Serial.println("Firmware URL: " + fwUrl);
  Serial.print("Firmware Size in bytes: ");
  Serial.println(fwSize);
  Serial.println("SHA: " + shaExpected);
  Serial.println("Manifest signature: " + signature);

  doc.remove("signature");
  String origManifest;
  serializeJson(doc, origManifest);

  // (Optional) verify manifest signature using firmwarePublicKey
  if (verifySignature(origManifest, signature, firmwarePublicKey)) {
        Serial.println("Manifest signature verified");
    } else {
        // Serial.println("Manifest signature invalid");
        logMessage("Error", "Manifest signature invalid");
        return 0;
    }

  // Step 2. Resume download if needed
  prefs.begin("fota", false);
  size_t offset = prefs.getULong("offset", 0);
  if (offset > 0) {
    offset = 0;
  };
  prefs.putInt("job_id", job_id);
  prefs.end();

  WiFiClientSecure fwClient;
  fwClient.setCACert(rootCACertificate);
  // fwClient.setInsecure(); // Preproduction only
  HTTPClient fwHttp;
  if (!fwHttp.begin(fwClient, fwUrl)) { 
    logMessage("Error", String("Unable to start http client on: ") + fwUrl);
    return 0;
  };
  char rangeHeader[64];
  sprintf(rangeHeader, "bytes=%u-", (unsigned int)offset);
  fwHttp.addHeader("Range", rangeHeader);
  int respCode = fwHttp.GET();

  if (respCode != HTTP_CODE_PARTIAL_CONTENT && respCode != HTTP_CODE_OK) {
    Serial.println(rangeHeader);
    Serial.printf("HTTP error: %d\n", respCode);
    logMessage("Error", String("Firmware HTTP response error with code:" + String(respCode)));
    fwHttp.end();
    return 0;
  }

  int totalWritten = offset;
  int totalSize = fwSize;
  WiFiClient *stream = fwHttp.getStreamPtr();

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);

  logMessage("Info", "Running partition:" + String(running->label) + ", Next OTA partition:" + String(next->label));

  esp_ota_handle_t otaHandle = 0;
  esp_err_t err = esp_ota_begin(next, fwSize, &otaHandle);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_begin failed: %s\n", esp_err_to_name(err));
    logMessage("Error", "Unable to allocate partition for new firmware");
    return 0;
  }

  // uint8_t buf[1024];
  uint8_t *buf = (uint8_t *)malloc(4096); // using heap since a bigger buffer size causes stack overflow
  if (!buf) {
    logMessage("Error", "Unable to allocate Heap buffer");
    esp_ota_end(otaHandle);
    return 0;
  }

  unsigned char hashBuf[32];
  size_t bytesRead;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);

  while (fwHttp.connected() && (bytesRead = stream->readBytes(buf, 4096)) > 0) {
    err = esp_ota_write(otaHandle, buf, bytesRead);
    if (err != ESP_OK) {
      Serial.printf("esp_ota_write failed: %s\n", esp_err_to_name(err));
      logMessage("Error", "Firmware chunk write failed");
      free(buf);
      esp_ota_end(otaHandle);
      return 0;
    }

    mbedtls_sha256_update_ret(&ctx, buf, bytesRead);
    totalWritten += bytesRead;

    // Persist progress (write offset to nvs every 100KB)
    if (totalWritten % 102400 == 0) {
      logMessage("Info", String(totalWritten) + " Bytes Written");
      prefs.begin("fota", false);
      prefs.putULong("offset", totalWritten);
      prefs.end();
    };

    Serial.printf("Progress: %d/%d bytes (%.2f%%)\r\n", totalWritten, fwSize, 100.0 * totalWritten / fwSize);
  }

  logMessage("Info", "Firmware Writing finished");
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
  // Step 4. Verify hash
  if (!computedHash.equalsIgnoreCase(shaExpected)) {   
    logMessage("Error", "SHA mismatch, abort");
    Serial.println("Computed Hash:" + computedHash);
    esp_ota_abort(otaHandle);
    return 0;
  }
  logMessage("Info", "SHA Verified");

  err = esp_ota_end(otaHandle);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_end failed: %s\n", esp_err_to_name(err));
    logMessage("Error", "Invalid Firmware");
    return 0;
  }

  err = esp_ota_set_boot_partition(next);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
    logMessage("Error", "Unable to select the new boot partition");
    return 0;
  }

  prefs.begin("fota", false);
  prefs.putULong("offset", 0);
  prefs.end();

  // Step 5. Report success
  logMessage("Info", "Firmware validated and ready for booting");
  return true;
}

void perform_FOTA_with_logging(){
  configTime(19800, 0, NTP_SERVER); // Adjusted for SL
  logMessage("Info", "Starting FOTA");
  bool restart = perform_FOTA(); 
  if (!restart){
    logMessage("Error", "FOTA Failed");
  };
  store_and_upload_log(); 
  if (restart) {
    Serial.println("Restarting in 1000 ms");
    delay(1000);
    ESP.restart();
  };
};