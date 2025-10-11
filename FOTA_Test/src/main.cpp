#include <Arduino.h>
#include <WiFi.h>
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

// Constants
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";
const char *manifestURL = "https://eco-watt-cloud.vercel.app/api/fota/manifest";

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

void performFOTA() {
  Preferences prefs;
  WiFiClientSecure client;
  HTTPClient http;
  client.setCACert(rootCACertificate); // Replace the certificate in case it expires
  // client.setInsecure();             // Use only for preproduction testing

  // Step 1. Download manifest
  // Remove client when CA certificate verification is not needed
  if (!http.begin(client, manifestURL)) { 
    Serial.println("Unable to start http client on: ");
    Serial.print(manifestURL);
    return;
  }; 

  Serial.println("Root CA Certificate verified");
  int code = http.GET();
  if (code != 200) {
    Serial.println(code);
    Serial.println("http GET failed");
    return;
  };

  String payload = http.getString();
  Serial.println(payload);
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
    return;
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
        Serial.println("Manifest signature invalid");
        return;
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
    Serial.println("Unable to start http client on: ");
    Serial.print(fwUrl);
    return;
  };
  char rangeHeader[64];
  sprintf(rangeHeader, "bytes=%u-", (unsigned int)offset);
  fwHttp.addHeader("Range", rangeHeader);
  int respCode = fwHttp.GET();

  if (respCode != HTTP_CODE_PARTIAL_CONTENT && respCode != HTTP_CODE_OK) {
    Serial.println(rangeHeader);
    Serial.printf("HTTP error: %d\n", respCode);
    fwHttp.end();
    return;
  }

  int totalWritten = offset;
  int totalSize = fwSize;
  WiFiClient *stream = fwHttp.getStreamPtr();

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);

  Serial.printf("Running partition: %s, Next OTA partition: %s\r\n", running->label, next->label);

  // Step 3. Begin OTA update
  // if (!Update.begin(totalSize)) {
  //   Serial.println("Update begin failed!");
  //   return;
  // }

  esp_ota_handle_t otaHandle = 0;
  esp_err_t err = esp_ota_begin(next, fwSize, &otaHandle);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_begin failed: %s\n", esp_err_to_name(err));
    return;
  }

  // uint8_t buf[1024];
  uint8_t *buf = (uint8_t *)malloc(4096); // using heap since a bigger buffer size causes stack overflow
  if (!buf) {
    Serial.println("Buffer allocation failed!");
    esp_ota_end(otaHandle);
    return;
  }

  unsigned char hashBuf[32];
  size_t bytesRead;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);

  while (fwHttp.connected() && (bytesRead = stream->readBytes(buf, 4096)) > 0) {
    // Update.write(buf, bytesRead);
    err = esp_ota_write(otaHandle, buf, bytesRead);
    if (err != ESP_OK) {
      Serial.printf("esp_ota_write failed: %s\n", esp_err_to_name(err));
      free(buf);
      esp_ota_end(otaHandle);
      return;
    }

    mbedtls_sha256_update_ret(&ctx, buf, bytesRead);
    totalWritten += bytesRead;

    // Persist progress (write offset to nvs every 100KB)
    if (totalWritten % 102400 == 0) {
      Serial.println("Saving the progress in nvs");
      prefs.begin("fota", false);
      prefs.putULong("offset", totalWritten);
      prefs.end();
    };
    Serial.printf("Progress: %d/%d bytes (%.2f%%)\r\n", totalWritten, fwSize, 100.0 * totalWritten / fwSize);

    // Report progress to cloud
    // reportProgress(job_id, totalWritten, totalSize);
  }
  Serial.println("Saving the progress in nvs");
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
  // String computedHash = toHex(hashBuf, 32);
  if (!computedHash.equalsIgnoreCase(shaExpected)) {
    Serial.println("SHA mismatch, abort");
    Serial.println(computedHash);
    esp_ota_abort(otaHandle);
    // Update.abort();
    // reportVerification(job_id, "failure", computedHash);
    return;
  }

  Serial.println("SHA Verified");

  // if (!Update.end(false)) { // true = set as boot partition
  //   Serial.printf("Update failed: %s\n", Update.errorString());
  //   return;
  // }
  err = esp_ota_end(otaHandle);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_end failed: %s\n", esp_err_to_name(err));
    return;
  }

  err = esp_ota_set_boot_partition(next);
  if (err != ESP_OK) {
    Serial.printf("esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
    return;
  }

  Serial.println("Update Writing Finished");
  prefs.begin("fota", false);
  prefs.putULong("offset", 0);
  prefs.end();

  // // Step 5. Report success
  // reportVerification(job_id, "success", computedHash);
  // reportRebootPending(job_id);

  Serial.println("Restarting in 1000 ms");
  delay(1000);
  ESP.restart(); // Controlled reboot
}

// void reportProgress(String job_id, size_t offset, size_t total) {
//   // Send HTTPS or MQTT JSON to EcoWatt Cloud
// }

// void reportVerification(String job_id, String result, String hash) {
//   // Send HTTPS POST to EcoWatt Cloud
// }

// void reportRebootPending(String job_id) {
//   // Send final status to EcoWatt Cloud before reboot
// }

bool wifi_init(void) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("\nWiFi connection failed");
        return false;
    }
}

void setup() {
  Serial.begin(115200);
  wifi_init();
  performFOTA();
  Serial.println("Exited FOTA");
}

void loop() {
}