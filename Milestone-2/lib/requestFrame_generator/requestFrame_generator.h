#ifndef REQUESTFRAME_GENERATOR_H
#define REQUESTFRAME_GENERATOR_H

#include <Arduino.h>
#include <vector>

// Function to generate the request frame
String generateRequestFrame(uint8_t slaveAddress, uint8_t functionCode, const std::vector<uint16_t>& registers);

#endif // REQUESTFRAME_GENERATOR_H