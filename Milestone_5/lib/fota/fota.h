#ifndef FOTA_H
#define FOTA_H

#include <Arduino.h>

// Function that accepts manifest parameters from cloud response
bool perform_FOTA_with_manifest(int job_id, 
                                const String& fwUrl, 
                                size_t fwSize, 
                                const String& shaExpected, 
                                const String& signature);

#endif