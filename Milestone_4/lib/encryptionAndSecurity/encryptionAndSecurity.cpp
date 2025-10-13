#include "encryptionAndSecurity.h"
#include "config.h"
#include <EEPROM.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

/**
 * @brief Encodes a byte array into a Base64 String using mbedtls.
 * @param payload Pointer to the byte array.
 * @param length The length of the byte array.
 * @return The Base64 encoded String.
 */
String encodeBase64(const uint8_t* payload, size_t length) {
    size_t encodedLen = 0;
    // First call to get the required length
    mbedtls_base64_encode(NULL, 0, &encodedLen, payload, length);

    char encodedPayload[encodedLen + 1]; // +1 for null terminator

    // Second call to perform the encoding
    mbedtls_base64_encode((unsigned char*)encodedPayload, encodedLen, &encodedLen, payload, length);
    
    encodedPayload[encodedLen] = '\0'; // Ensure null termination
    return String(encodedPayload);
}

/**
 * @brief Decodes a Base64 String into a byte array using mbedtls.
 * @param encodedPayload The Base64 encoded String.
 * @param outputBuffer Pointer to the buffer where decoded data will be stored.
 * @param outputBufferSize The size of the output buffer.
 * @return The number of bytes decoded, or 0 on error.
 */
size_t decodeBase64(const String& encodedPayload, uint8_t* outputBuffer, size_t outputBufferSize) {
    size_t decodedLen = 0;
    int ret = mbedtls_base64_decode(
        outputBuffer, 
        outputBufferSize, 
        &decodedLen, 
        (const unsigned char*)encodedPayload.c_str(), 
        encodedPayload.length()
    );

    if (ret == 0) {
        return decodedLen; // Success
    } else {
        return 0; // Fail
    }
}

/**
 * @brief Generate HMAC-SHA256 MAC for the given payload using PSK from config.
 * @param payload The message to authenticate.
 * @return String containing the MAC in hexadecimal format.
 */
String generateMAC(const String& payload) {
    return generateMAC((const uint8_t*)payload.c_str(), payload.length());
}

/**
 * @brief Generates an HMAC-SHA256 MAC for a byte array using mbedtls.
 * @param payload Pointer to the byte array.
 * @param length The length of the byte array.
 * @return The MAC as a hexadecimal String.
 */
String generateMAC(const uint8_t* payload, size_t length) {
    uint8_t mac[32]; // SHA-256 outputs a 32-byte hash
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    // Perform the HMAC operation
    mbedtls_md_hmac(
        md_info,
        (const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK),
        payload, length,
        mac
    );

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

/**
 * @brief Initializes the EEPROM for nonce storage.
 */
void NonceManager::begin() {
    EEPROM.begin(4); 
}

/**
 * @brief Gets the current nonce value and increments it for the next use.
 * @return The current nonce value (before incrementing).
 */

uint32_t NonceManager::getAndIncrementNonce() {
    uint32_t currentNonce;
    EEPROM.get(NONCE_ADDRESS, currentNonce);
    uint32_t nextNonce = currentNonce + 1;
    EEPROM.put(NONCE_ADDRESS, nextNonce);
    EEPROM.commit();
    return currentNonce;
}

