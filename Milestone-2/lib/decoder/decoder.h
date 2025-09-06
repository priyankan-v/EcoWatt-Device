#ifndef DECODER_H
#define DECODER_H

#include <Arduino.h>
#include <vector>

// Function to decode the Modbus response frame and return register values
std::vector<uint16_t> decodeResponse(const String& responseFrame);

#endif // DECODER_H