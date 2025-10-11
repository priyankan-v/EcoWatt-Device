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
 
  if (random(2)) {
    Serial.println("Marking this firmware as safe");
    esp_ota_mark_app_valid_cancel_rollback();
    esp_ota_get_state_partition(running, &ota_state);
    otaState = ota_state == ESP_OTA_IMG_NEW ? "ESP_OTA_IMG_NEW"
            : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
            : ota_state == ESP_OTA_IMG_VALID ? "ESP_OTA_IMG_VALID"
            : ota_state == ESP_OTA_IMG_INVALID ? "ESP_OTA_IMG_INVALID"
            : ota_state == ESP_OTA_IMG_ABORTED ? "ESP_OTA_IMG_ABORTED"
            : "ESP_OTA_IMG_UNDEFINED";
    Serial.println(otaState);
    return;
  } else {
    Serial.println("Marking this firmware as unsafe and reverting");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    esp_ota_get_state_partition(running, &ota_state);
    otaState = ota_state == ESP_OTA_IMG_NEW ? "ESP_OTA_IMG_NEW"
            : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
            : ota_state == ESP_OTA_IMG_VALID ? "ESP_OTA_IMG_VALID"
            : ota_state == ESP_OTA_IMG_INVALID ? "ESP_OTA_IMG_INVALID"
            : ota_state == ESP_OTA_IMG_ABORTED ? "ESP_OTA_IMG_ABORTED"
            : "ESP_OTA_IMG_UNDEFINED";
    Serial.println(otaState);
  };

  Serial.println("Restarting in 2000 ms");
  delay(2000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  reportStatus();
  Serial.println("New firmware finalized");
}

void loop() {
}