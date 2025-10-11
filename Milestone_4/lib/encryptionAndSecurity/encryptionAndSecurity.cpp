#include "encryptionAndSecurity.h"
#include "config.h"
#include <EEPROM.h>
#include <Hash.h>
#include <SHA256.h>
#include "base64.hpp"

// Helper macro to calculate the required buffer size for base64 encoding
#define BASE64_ENCODE_OUT_SIZE(s) (((s) + 2) / 3 * 4)
#define BASE64_DECODE_OUT_SIZE(s) ((s) / 4 * 3)

/**
 * @brief Encodes a byte array into a Base64 String.
 * @param payload Pointer to the byte array.
 * @param length The length of the byte array.
 * @return The Base64 encoded String.
 */
String encodeBase64(const uint8_t* payload, size_t length) {
    size_t encodedLen = BASE64_ENCODE_OUT_SIZE(length);
    char encodedPayload[encodedLen + 1]; // +1 for null terminator

    encode_base64((unsigned char*)payload, length, (unsigned char*)encodedPayload);
    
    encodedPayload[encodedLen] = '\0'; // Ensure null termination for the String object
    return String(encodedPayload);
}

/**
 * @brief Decodes a Base64 String into a byte array.
 * @param encodedPayload The Base64 encoded String.
 * @param outputBuffer Pointer to the buffer where decoded data will be stored.
 * @param outputBufferSize The size of the output buffer.
 * @return The number of bytes decoded, or 0 on error (e.g., buffer too small).
 */
size_t decodeBase64(const String& encodedPayload, uint8_t* outputBuffer, size_t outputBufferSize) {
    // Calculate the maximum possible decoded length
    size_t expectedLen = BASE64_DECODE_OUT_SIZE(encodedPayload.length());
    if (outputBufferSize < expectedLen) {
        // Error: Output buffer is too small to hold the decoded data.
        return 0; 
    }

    // Use the decode function from the base64 library
    // It returns the actual number of bytes written to the output buffer.
    size_t decodedLen = decode_base64(
        (const unsigned char*)encodedPayload.c_str(), 
        encodedPayload.length(), 
        outputBuffer
    );
    
    return decodedLen;
}

/**
 * @brief Generate HMAC-SHA256 MAC for the given payload using PSK from config.
 * @param payload The message to authenticate.
 * @return String containing the MAC in hexadecimal format.
 */
String generateMAC(const String& payload) {
    return generateMAC(payload.c_str());
}

/**
 * @brief Generate HMAC-SHA256 MAC for the given payload using PSK from config.
 * @param payload The message to authenticate (C-style string).
 * @return String containing the MAC in hexadecimal format.
 */
String generateMAC(const char* payload) {
    // Create a buffer for the binary MAC result
    uint8_t mac[SHA256::HASH_SIZE];

    hmac<SHA256>(mac, sizeof(mac),
                 (const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK),
                 (const uint8_t*)payload, strlen(payload));

    // Convert the binary MAC to a hexadecimal string
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

// --- NonceManager Implementation ---

#define NONCE_ADDRESS 0

void NonceManager::begin() {
    EEPROM.begin(4); 
}

uint32_t NonceManager::getAndIncrementNonce() {
    uint32_t currentNonce;
    EEPROM.get(NONCE_ADDRESS, currentNonce);
    uint32_t nextNonce = currentNonce + 1;
    EEPROM.put(NONCE_ADDRESS, nextNonce);
    EEPROM.commit();
    return currentNonce;
}

