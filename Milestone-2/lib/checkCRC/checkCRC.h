#ifndef CHECKCRC_H
#define CHECKCRC_H

#include <cstdint>
#include <Arduino.h>

bool validateCRC(const String& responseFrame);

#endif