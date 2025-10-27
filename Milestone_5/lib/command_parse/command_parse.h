#ifndef COMMAND_PARSE_H
#define COMMAND_PARSE_H

#include <Arduino.h>

bool extract_command(const String& response, String& action, uint16_t& target_register, uint16_t& value);

#endif // COMMAND_PARSE_H