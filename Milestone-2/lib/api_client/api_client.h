#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>

bool api_init(void);
String api_send_request(const String& endpoint, const String& frame);

#endif