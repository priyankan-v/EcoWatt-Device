#include "encryptionAndSecurity.h"
#include "../config/config.h"
#include <Crypto.h>
#include <SHA256.h>
#include <HMAC.h>
#include <Base64.h>
#include <EEPROM.h>

/**
 * @brief Encodes a byte array into a Base64 String.
 * * @param payload Pointer to the byte array.
 * @param length The length of the byte array.
 * @return The Base64 encoded String.
 */
String encodeBase64(const uint8_t* payload, size_t length) {
    int encodedLen = Base64.encodedLength(length);
    char encodedPayload[encodedLen + 1]; // +1 for null terminator

    // cast the uint8_t* to char* for the Base64 library.
    Base64.encode(encodedPayload, (char*)payload, length);
    encodedPayload[encodedLen] = '\0'; // Ensure null termination for the String object

    return String(encodedPayload);
}

/**
 * Generate HMAC-SHA256 MAC for the given payload using PSK from config
 * @param payload The message to authenticate
 * @return String containing the MAC in hexadecimal format
 */
String generateMAC(const String& encodedPayload) {
   return generateMAC(encodedPayload.c_str());
}

/**
 * Generate HMAC-SHA256 MAC for the given encodedPayload using PSK from config
 * @param encodedPayload The message to authenticate (C-style string)
 * @return String containing the MAC in hexadecimal format
 */
String generateMAC(const char* encodedPayload) {

    HMAC<SHA256> hmac;

    // Set the secret key (UPLOAD_PSK) for the HMAC calculation
    hmac.setKey((const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK));
    
    hmac.update((const uint8_t*)encodedPayload, strlen(encodedPayload));

    // Finalize and get the 32-byte result (the MAC)
    uint8_t mac[SHA256::HASH_SIZE];
    hmac.finalize(mac, sizeof(mac));

    // Convert MAC to hexadecimal string
    String macHex = "";
    for (size_t i = 0; i < sizeof(mac); i++) {
        if (mac[i] < 0x10) {
            macHex += "0"; // Add a leading zero if needed
        }
        macHex += String(mac[i], HEX);
    }
    return macHex;
}


void NonceManager::begin() {
    // Allocate 4 bytes to store the nonce (a 32-bit unsigned long)
    EEPROM.begin(4); 
}

uint32_t NonceManager::getAndIncrementNonce() {
    // 1. Read the current nonce from EEPROM
    uint32_t currentNonce;
    EEPROM.get(NONCE_ADDRESS, currentNonce);

    // 2. Increment the nonce for the next transaction
    uint32_t nextNonce = currentNonce + 1;

    // 3. Save the new nonce back to EEPROM
    EEPROM.put(NONCE_ADDRESS, nextNonce);
    EEPROM.commit(); // Make sure the data is written to flash

    return currentNonce;
}



// /**
//  * @brief Generates an HMAC-SHA256 MAC for a byte array.
//  * * @param payload Pointer to the byte array.
//  * @param length The length of the byte array.
//  * @return The MAC as a hexadecimal String.
//  */
// String generateMAC(const uint8_t* payload, size_t length) {
//     HMAC<SHA256> hmac;
    
//     // Set the secret key (UPLOAD_PSK) for the HMAC calculation
//     hmac.setKey((const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK));
    
//     // Pass the message payload (uint8_t*) to the HMAC object
//     hmac.update(payload, length);
    
//     // Finalize and get the 32-byte result (the MAC)
//     uint8_t mac[SHA256::HASH_SIZE];
//     hmac.finalize(mac, sizeof(mac));
    
//     // Convert the binary MAC to a hexadecimal string
//     String macHex = "";
//     macHex.reserve(SHA256::HASH_SIZE * 2); // Pre-allocate memory for efficiency
//     for (size_t i = 0; i < sizeof(mac); i++) {
//         if (mac[i] < 0x10) {
//             macHex += "0"; // Add a leading zero if needed
//         }
//         macHex += String(mac[i], HEX);
//     }
    
//     return macHex;
// }