#include "command_parse.h"
#include "config.h"

bool extract_command(const String& response, String& action, uint16_t& target_register, uint16_t& value) {
    // Basic validation
    if (response.length() == 0) {
        Serial.println(F("Error: Empty response received from the cloud API."));
        return false;
    }

    // Check for command block
    int cmd_index = response.indexOf(F("\"command\""));
    if (cmd_index >= 0) {
        Serial.println(F("Command section detected in response."));

        // Extract 'action'
        int action_start = response.indexOf(F("\"action\":\""), cmd_index);
        if (action_start >= 0) {
            int value_start = action_start + strlen("\"action\":\"");
            int value_end = response.indexOf(F("\""), value_start);
            if (value_end > value_start) {
                action = response.substring(value_start, value_end);
                if(!(action.equalsIgnoreCase("write_register") ||
                     action.equalsIgnoreCase("read_register"))) {
                    Serial.println(F("Error: Unsupported action command received"));
                    return false;
                }
                Serial.print(F("Command Action: "));
                Serial.println(action);
            }
        }

        // Extract 'target_register'
        int target_start = response.indexOf(F("\"target_register\":\""), cmd_index);
        if (target_start >= 0) {
            int value_start = target_start + strlen("\"target_register\":\"");
            int value_end = response.indexOf(F("\""), value_start);
            if (value_end > value_start) {
                String target_reg = response.substring(value_start, value_end);
                target_reg.trim();
                target_register = target_reg.toInt();
            }
        }

        // Extract 'value'
        if (action.equalsIgnoreCase("write_register")) {
            int value_start = response.indexOf(F("\"value\":"), cmd_index);
            if (value_start >= 0) {
                // Move past "value":
                value_start += strlen("\"value\":");
                // Find the next comma or closing brace after the value
                int value_end = response.indexOf(F(","), value_start);
                if (value_end < 0) value_end = response.indexOf(F("}"), value_start);
                if (value_end > value_start) {
                    String value_str = response.substring(value_start, value_end);
                    value_str.trim();
                    value = value_str.toInt();
                }
            }
        }

        return true;

    }else{
        Serial.println(F("No command section found in response."));
        return false;
    }
}