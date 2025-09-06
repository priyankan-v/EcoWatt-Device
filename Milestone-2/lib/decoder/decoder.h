#ifndef DECODER_H
#define DECODER_H

#include <Arduino.h>
#include <vector>

std::vector<uint16_t> decodeResponse(const String& responseFrame);

#endif