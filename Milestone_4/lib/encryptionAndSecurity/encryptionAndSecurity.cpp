#include "encryptionAndSecurity.h"
#include "config.h"
#include <LittleFS.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/sha256.h>

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

/**
 * @brief Encrypts payload using AES-256-CBC with random IV generation.
 * @param plaintext Pointer to the plaintext data.
 * @param plaintext_len Length of the plaintext.
 * @param ciphertext Pointer to buffer for encrypted output (must be large enough).
 * @param ciphertext_len Pointer to store the actual ciphertext length.
 * @param iv_output Pointer to 16-byte buffer to store the generated IV.
 * @return true on success, false on failure.
 */
bool encryptPayloadAES_CBC(const uint8_t* plaintext, size_t plaintext_len,
                           uint8_t* ciphertext, size_t* ciphertext_len,
                           uint8_t* iv_output) {
    
    // Step 1: Derive 32-byte AES-256 key from PSK using SHA-256
    uint8_t aes_key[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts_ret(&sha_ctx, 0); // 0 = SHA-256
    mbedtls_sha256_update_ret(&sha_ctx, (const uint8_t*)UPLOAD_PSK, strlen(UPLOAD_PSK));
    mbedtls_sha256_finish_ret(&sha_ctx, aes_key);
    mbedtls_sha256_free(&sha_ctx);
    
    Serial.println(F("[ENCRYPTION] AES-256 key derived from PSK"));
    
    // Step 2: Generate random 16-byte IV
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char* personalization = "EcoWatt_AES_IV";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const uint8_t*)personalization, strlen(personalization));
    if (ret != 0) {
        Serial.printf("[ENCRYPTION] Failed to seed RNG: -0x%04X\n", -ret);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return false;
    }
    
    ret = mbedtls_ctr_drbg_random(&ctr_drbg, iv_output, 16);
    if (ret != 0) {
        Serial.printf("[ENCRYPTION] Failed to generate IV: -0x%04X\n", -ret);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return false;
    }
    
    Serial.print(F("[ENCRYPTION] Generated IV: "));
    for (int i = 0; i < 16; i++) {
        if (iv_output[i] < 0x10) Serial.print("0");
        Serial.print(iv_output[i], HEX);
    }
    Serial.println();
    
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    
    // Step 3: Add PKCS#7 padding
    size_t padding_len = 16 - (plaintext_len % 16);
    size_t padded_len = plaintext_len + padding_len;
    uint8_t padded_plaintext[padded_len];
    
    memcpy(padded_plaintext, plaintext, plaintext_len);
    for (size_t i = plaintext_len; i < padded_len; i++) {
        padded_plaintext[i] = padding_len; // PKCS#7 padding byte
    }
    
    Serial.printf("[ENCRYPTION] Plaintext: %d bytes, Padded: %d bytes (padding: %d)\n", 
                  plaintext_len, padded_len, padding_len);
    
    // Step 4: Encrypt using AES-256-CBC
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    ret = mbedtls_aes_setkey_enc(&aes, aes_key, 256);
    if (ret != 0) {
        Serial.printf("[ENCRYPTION] Failed to set AES key: -0x%04X\n", -ret);
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Copy IV for encryption (mbedtls_aes_crypt_cbc modifies IV)
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv_output, 16);
    
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len,
                                iv_copy, padded_plaintext, ciphertext);
    
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        Serial.printf("[ENCRYPTION] AES encryption failed: -0x%04X\n", -ret);
        return false;
    }
    
    *ciphertext_len = padded_len;
    
    Serial.printf("[ENCRYPTION] Encryption successful: %d bytes ciphertext\n", *ciphertext_len);
    
    return true;
}


// --- NonceManager Implementation (LittleFS-based) ---

/**
 * @brief Initializes LittleFS and creates nonce file if it doesn't exist.
 */
void NonceManager::begin() {
    // Mount LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println(F("[NONCE] Failed to mount LittleFS"));
        return;
    }
    
    Serial.println(F("[NONCE] LittleFS mounted successfully"));
    
    // Check if nonce file exists
    if (!LittleFS.exists(nonce_file)) {
        Serial.println(F("[NONCE] Nonce file not found, creating with initial value 0"));
        File file = LittleFS.open(nonce_file, "w");
        if (file) {
            file.print("0");
            file.close();
            Serial.println(F("[NONCE] Nonce file created"));
        } else {
            Serial.println(F("[NONCE] Failed to create nonce file"));
        }
    } else {
        Serial.println(F("[NONCE] Nonce file exists"));
    }
}

/**
 * @brief Gets the current nonce value and increments it for the next use.
 * @return The current nonce value (before incrementing).
 */
uint32_t NonceManager::getAndIncrementNonce() {
    uint32_t currentNonce = 0;
    
    // Read current nonce from file
    File file = LittleFS.open(nonce_file, "r");
    if (file) {
        String nonceStr = file.readString();
        currentNonce = nonceStr.toInt();
        file.close();
    } else {
        Serial.println(F("[NONCE] Failed to read nonce file, using 0"));
    }
    
    // Increment and write back
    uint32_t nextNonce = currentNonce + 1;
    file = LittleFS.open(nonce_file, "w");
    if (file) {
        file.print(nextNonce);
        file.close();
    } else {
        Serial.println(F("[NONCE] Failed to write nonce file"));
    }
    
    return currentNonce;
}

