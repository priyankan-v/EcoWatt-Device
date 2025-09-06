#include "checkCRC.h"
#include "calculateCRC.h"

bool checkCRC(const String& responseFrame) {
    if (responseFrame.length() < 8) {
        Serial.println("Response frame too short for CRC validation.");
        return false;
    }

    int frameLength = responseFrame.length() / 2;
    uint8_t responseBytes[frameLength];
    for (int i = 0; i < frameLength; ++i) {
        responseBytes[i] = strtoul(responseFrame.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }

    uint16_t receivedCRC = (responseBytes[frameLength - 2]) | (responseBytes[frameLength - 1] << 8);

    // Calculate CRC on all bytes except the last two(CRC bytes)
    uint16_t calculatedCRC = calculateCRC(responseBytes, frameLength - 2); 

    Serial.print("Calculated CRC: ");
    Serial.println(calculatedCRC, HEX);
    Serial.print("Received CRC: ");
    Serial.println(receivedCRC, HEX);

    return calculatedCRC == receivedCRC;
}