#ifndef CHECKCRC_H
#define CHECKCRC_H

#include <cstdint>
#include <Arduino.h>

bool checkCRC(const String& responseFrame);

#endif