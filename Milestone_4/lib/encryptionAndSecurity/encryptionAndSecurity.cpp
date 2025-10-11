#include "encryptionAndSecurity.h"
#include "config.h"
#include <EEPROM.h>
#include <Crypto.h>
#include <SHA256.h>
#include "base64.hpp"

// Helper macro to calculate the required buffer size for base64 encoding
#define BASE64_ENCODE_OUT_SIZE(s) (((s) + 2) / 3 * 4)

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
    // 1. Create an object of the SHA256 class
    SHA256 sha256;

    // 2. Initialize the HMAC calculation with the key
    sha256.resetHMAC((const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK));
    
    // 3. Update with the payload to be authenticated
    sha256.update((const uint8_t*)payload, strlen(payload));

    // 4. Finalize and get the HMAC result
    uint8_t mac[SHA256::HASH_SIZE];
    sha256.finalizeHMAC((const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK), mac, sizeof(mac));

    // 5. Convert the binary MAC to a hexadecimal string
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

