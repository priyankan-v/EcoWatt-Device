#ifndef CLOUD_API_HANDLER_H
#define CLOUD_API_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

bool validate_upload_response(const String& response);
bool parse_config_update_from_response(const String& response, String& config_update_json);
void send_config_ack_to_cloud(const String& ack_json);
bool parse_fota_manifest_from_response(const String& response, 
                                       int& job_id, 
                                       String& fwUrl, 
                                       size_t& fwSize, 
                                       String& shaExpected, 
                                       String& signature);
void encrypt_compressed_frame(const uint8_t* data, size_t len, uint8_t* output_data);
void calculate_and_add_mac(const uint8_t* data, size_t len, uint8_t* mac_output);
void append_crc_to_upload_frame(const uint8_t* encrypted_frame, size_t frame_length, uint8_t* output_frame);

#endif