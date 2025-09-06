#include "decoder.h"

std::vector<uint16_t> decodeResponse(const String& responseFrame) {
    std::vector<uint16_t> registerValues;

    // Ensure the response frame is valid
    if (responseFrame.length() < 8) { // Minimum frame length (Slave Address, Function Code, Byte Count, CRC)
        Serial.println("Response frame too short for decoding.");
        return registerValues;
    }

    // Convert the response frame to a byte array
    int frameLength = responseFrame.length() / 2;
    uint8_t responseBytes[frameLength];
    for (int i = 0; i < frameLength; ++i) {
        responseBytes[i] = strtoul(responseFrame.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }

    // Extract Byte Count
    uint8_t byteCount = responseBytes[2];

    // Ensure the Byte Count matches the data length
    if (frameLength < 3 + byteCount + 2) { // 3 bytes (Slave Address, Function Code, Byte Count) + Data + 2 bytes CRC
        Serial.println("Invalid Byte Count or frame length.");
        return registerValues;
    }

    // Decode register values (each register is 2 bytes)
    for (int i = 0; i < byteCount; i += 2) {
        uint16_t registerValue = (responseBytes[3 + i] << 8) | responseBytes[3 + i + 1];
        registerValues.push_back(registerValue);
    }

    return registerValues;
}