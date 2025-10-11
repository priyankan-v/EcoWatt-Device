#ifndef ENCRYPTION_AND_SECURITY_H
#define ENCRYPTION_AND_SECURITY_H

#include <Arduino.h>

// Function declarations
// String generateMAC(const uint8_t* payload, size_t length);
String encodeBase64(const uint8_t* payload, size_t length);
size_t decodeBase64(const String& encodedPayload, uint8_t* outputBuffer, size_t outputBufferSize);
String generateMAC(const String& encodedPayload);
String generateMAC(const char* encodedPayload);

// Define the EEPROM address where the nonce will be stored.
// Pick an address that doesn't conflict with other data.
#define NONCE_ADDRESS 0

class NonceManager {
public:
    /**
     * @brief Initializes the EEPROM for nonce storage. Call this in setup().
     */
    void begin();

    /**
     * @brief Gets the current nonce value and increments it for the next use.
     * @return The current nonce value (before incrementing).
     */
    uint32_t getAndIncrementNonce();
};

#endif