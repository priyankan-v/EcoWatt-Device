#ifndef ENCRYPTION_AND_SECURITY_H
#define ENCRYPTION_AND_SECURITY_H

#include <Arduino.h>

// Function declarations
// String generateMAC(const uint8_t* payload, size_t length);
String encodeBase64(const uint8_t* payload, size_t length);
size_t decodeBase64(const String& encodedPayload, uint8_t* outputBuffer, size_t outputBufferSize);
String generateMAC(const String& encodedPayload);
String generateMAC(const uint8_t* payload, size_t length);

class NonceManager {
public:
    void begin();
    uint32_t getAndIncrementNonce();
};

#endif