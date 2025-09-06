#include <Arduino.h>
#include <wifi_manager.h>
#include <api_client.h>
#include <calculateCRC.h>
#include <checkCRC.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("EcoWatt!!!");

  if (!wifi_init()) {
        Serial.println("Failed to initialize WiFi. Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
  
    // Test api_client
    if (api_init()) {
      String endpoint = "read";
      String frame = "110300000002C69B";
      String result = api_send_request(endpoint, frame);
      Serial.print("API Response: ");
      Serial.println(result);

      // CRC validation
      if (checkCRC(result)) {
        Serial.println("CRC check PASSED");
      } else {
        Serial.println("CRC check FAILED");
      }

    } else {
      Serial.println("API client initialization failed.");
    }
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}
