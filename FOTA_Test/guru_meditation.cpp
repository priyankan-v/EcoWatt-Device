#include <Arduino.h>
#include <Preferences.h>
#include "esp_ota_ops.h" //remove after debugging
#include "esp_partition.h"

void reportStatus() {
  Serial.println("New firmware booted");

  Preferences prefs;
  prefs.begin("fota", false);
  size_t offset = prefs.getULong("offset", 0);
  prefs.end();
  Serial.printf("Checking the NVS, Total Written:%d\r\n", offset);

  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t ota_state;
  esp_ota_get_state_partition(running, &ota_state);

  const char* otaState = ota_state == ESP_OTA_IMG_NEW ? "ESP_OTA_IMG_NEW"
            : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
            : ota_state == ESP_OTA_IMG_VALID ? "ESP_OTA_IMG_VALID"
            : ota_state == ESP_OTA_IMG_INVALID ? "ESP_OTA_IMG_INVALID"
            : ota_state == ESP_OTA_IMG_ABORTED ? "ESP_OTA_IMG_ABORTED"
            : "ESP_OTA_IMG_UNDEFINED";

  Serial.printf("Running partition: %s, Next OTA partition: %s\r\n", running->label, next->label);
  Serial.println(otaState);
 
  Serial.println("Triggering Guru Meditation");
  int *ptr = NULL;
  *ptr = 1;  // Force a crash (invalid memory access)
}

void setup() {
  Serial.begin(115200);
  reportStatus();
  Serial.println("New firmware finalized");
}

void loop() {
}