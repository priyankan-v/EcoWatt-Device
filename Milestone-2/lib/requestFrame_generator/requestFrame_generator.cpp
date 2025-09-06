#include "requestFrame_generator.h"
#include "calculateCRC.h"

String generateRequestFrame(uint8_t slaveAddress, uint8_t functionCode, const std::vector<uint16_t>& registers) {
    if (functionCode != 0x03 && functionCode != 0x06) {
        Serial.println("Unsupported function code. Only 0x03 and 0x06 are supported.");
        return "";
    }

    if (registers.empty()) {
        Serial.println("Register array is empty.");
        return "";
    }

    char frameWithoutCRC[13]; // Buffer for the frame without CRC

    if (functionCode == 0x03) {
        uint16_t startRegister = registers.front();
        uint16_t numRegisters = registers.size();

        snprintf(frameWithoutCRC, sizeof(frameWithoutCRC), "%02X%02X%04X%04X", slaveAddress, functionCode, startRegister, numRegisters);
    } else if (functionCode == 0x06) {
        if (registers.size() < 2) {
            Serial.println("Invalid registers array for function code 0x06. Must contain register address and value.");
            return "";
        }

        uint16_t registerAddress = registers[0];
        uint16_t value = registers[1];

        snprintf(frameWithoutCRC, sizeof(frameWithoutCRC), "%02X%02X%04X%04X", slaveAddress, functionCode, registerAddress, value);
    }

    // Convert the frameWithoutCRC to a string of hex characters
    String frameWithoutCRCStr = String(frameWithoutCRC);
    return frameWithoutCRCStr;
}