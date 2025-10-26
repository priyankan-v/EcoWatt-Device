#ifndef ENCRYPTION_AND_SECURITY_H
#define ENCRYPTION_AND_SECURITY_H

#include <Arduino.h>

// Base64 encoding/decoding
String encodeBase64(const uint8_t* payload, size_t length);
size_t decodeBase64(const String& encodedPayload, uint8_t* outputBuffer, size_t outputBufferSize);

// MAC generation (HMAC-SHA256)
String generateMAC(const String& encodedPayload);
String generateMAC(const uint8_t* payload, size_t length);

// AES-256-CBC Encryption
bool encryptPayloadAES_CBC(const uint8_t* plaintext, size_t plaintext_len,
                           uint8_t* ciphertext, size_t* ciphertext_len,
                           uint8_t* iv_output);

// Nonce management with LittleFS (wear-leveling)
class NonceManager {
private:
    const char* nonce_file = "/nonce.txt";
public:
    void begin();
    uint32_t getAndIncrementNonce();
};

#endif