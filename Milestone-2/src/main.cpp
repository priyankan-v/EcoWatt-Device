#include <Arduino.h>
#include <wifi_manager.h>



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("EcoWatt!!!");

  if (!wifi_init()) {
        Serial.println("Failed to initialize WiFi. Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}
