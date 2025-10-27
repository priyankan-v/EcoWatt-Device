#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

// Runtime configuration structure
typedef struct {
    uint32_t sampling_interval_ms;
    uint32_t upload_interval_ms;
    uint8_t slave_address;
    uint8_t register_count;
    uint16_t active_registers[MAX_REGISTERS];
    bool config_valid;
} runtime_config_t;

// Configuration validation limits
typedef struct {
    uint32_t min_sampling_ms;
    uint32_t max_sampling_ms;
    uint32_t min_upload_ms;
    uint32_t max_upload_ms;
    uint8_t max_register_count;
} config_limits_t;

class ConfigManager {
private:
    runtime_config_t current_config;
    runtime_config_t pending_config;
    bool has_pending_config;
    config_limits_t limits;
    Preferences nvs;
    SemaphoreHandle_t config_mutex;
    bool initialized;
    static const char* NVS_NAMESPACE;
    
    // Internal methods
    void set_default_config();
    bool save_to_flash_unlocked();  // Version that doesn't acquire mutex
    bool validate_sampling_interval(uint32_t interval_ms);
    bool validate_upload_interval(uint32_t interval_ms);
    bool validate_slave_address(uint8_t addr);
    bool validate_registers(const JsonArray& registers);
    uint16_t get_register_address(const String& name);

public:
    ConfigManager();
    ~ConfigManager();
    
    bool load_from_flash();
    bool save_to_flash();
    runtime_config_t get_current_config();
    
    // Thread-safe getters
    uint32_t get_sampling_interval_ms();
    uint32_t get_upload_interval_ms();
    uint8_t get_slave_address();
    uint8_t get_register_count();
    void get_active_registers(uint16_t* registers, uint8_t max_count);
    
    bool init();
    bool is_initialized();
    
    // Pending configuration management
    bool has_pending_changes();
    void apply_pending_config();
    void clear_pending_config();
    
    // Cloud integration
    String process_cloud_config_update(const String& response);
    
    // Response generation
    String generate_config_ack(const JsonArray& accepted, const JsonArray& rejected, const JsonArray& unchanged);
};

// Global instance and functions
extern ConfigManager* g_config_manager;

// Global functions
bool config_manager_init();
runtime_config_t config_get_current();
uint32_t config_get_sampling_interval_ms();
uint32_t config_get_upload_interval_ms();
uint8_t config_get_slave_address();
uint8_t config_get_register_count();
void config_get_active_registers(uint16_t* registers, uint8_t max_count);

// Cloud integration functions
String config_process_cloud_response(const String& response);
bool config_has_pending_changes();
void config_apply_pending_changes();
void config_clear_pending_changes();

#endif