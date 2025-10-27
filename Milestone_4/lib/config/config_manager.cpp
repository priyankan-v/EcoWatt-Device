#include "config_manager.h"

// Static members
const char* ConfigManager::NVS_NAMESPACE = "device_config";
ConfigManager* g_config_manager = nullptr;

// Register name to address mapping
static const struct {
    const char* name;
    uint16_t address;
} REGISTER_MAP[] = {
    {"phase_voltage", 0x0000},
    {"phase_current", 0x0001},
    {"phase_frequency", 0x0002},
    {"pv1_voltage", 0x0003},
    {"pv2_voltage", 0x0004},
    {"pv1_current", 0x0005},
    {"pv2_current", 0x0006},
    {"inverter_temperature", 0x0007},
    {"export_power_percentage", 0x0008},
    {"output_power", 0x0009}
};
static const size_t REGISTER_MAP_SIZE = sizeof(REGISTER_MAP) / sizeof(REGISTER_MAP[0]);

// Semaphore timeout to prevent deadlocks
static const TickType_t CONFIG_MUTEX_TIMEOUT = pdMS_TO_TICKS(1000); // 1 second timeout

//construtor
ConfigManager::ConfigManager() : initialized(false), config_mutex(nullptr), has_pending_config(false) {
    // Set validation limits
    limits.min_sampling_ms = 1000;     // 1 second minimum
    limits.max_sampling_ms = 3600000;  // 1 hour maximum
    limits.min_upload_ms = 5000;       // 5 seconds minimum
    limits.max_upload_ms = 86400000;   // 24 hours maximum
    limits.max_register_count = MAX_REGISTERS;
    
    set_default_config();
}

// destructor - manual garbage collection
ConfigManager::~ConfigManager() {
    if (config_mutex != nullptr) {
        vSemaphoreDelete(config_mutex);
    }
    nvs.end();
}

void ConfigManager::set_default_config() {
    current_config.sampling_interval_ms = POLL_INTERVAL_MS;
    current_config.upload_interval_ms = UPLOAD_INTERVAL_MS;
    current_config.slave_address = SLAVE_ADDRESS;
    current_config.register_count = 4;
    
    // Default registers: Phase Voltage, Phase Current, Phase Frequency, Output Power
    current_config.active_registers[0] = 0x0000; // phase_voltage
    current_config.active_registers[1] = 0x0001; // phase_current
    current_config.active_registers[2] = 0x0002; // phase_frequency
    current_config.active_registers[3] = 0x0009; // output_power

    current_config.config_valid = true;
}

bool ConfigManager::init() {
    if (initialized) {
        return true;
    }
    
    // Create mutex
    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == nullptr) {
        Serial.println(F("[ConfigManager]: Failed to create mutex"));
        return false;
    }
    
    // Initialize NVS
    if (!nvs.begin(NVS_NAMESPACE, false)) {
        Serial.println(F("[ConfigManager]: Failed to initialize NVS"));
        return false;
    }
    
    // Load configuration from flash
    if (!load_from_flash()) {
        Serial.println(F("[ConfigManager]: Using default configuration"));
        save_to_flash(); // Save defaults
    }
    
    initialized = true;
    Serial.println(F("[ConfigManager]: Initialized successfully"));
    return true;
}

bool ConfigManager::load_from_flash() {
    if (!nvs.isKey("sampling_ms")) {
        return false; // No configuration exists
    }
    
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        // reading each config parameter from NVS
        current_config.sampling_interval_ms = nvs.getUInt("sampling_ms", POLL_INTERVAL_MS);
        current_config.upload_interval_ms = nvs.getUInt("upload_ms", UPLOAD_INTERVAL_MS);
        current_config.slave_address = nvs.getUChar("slave_addr", SLAVE_ADDRESS);
        current_config.register_count = nvs.getUChar("reg_count", 4);
        
        // Load active registers
        size_t reg_size = sizeof(current_config.active_registers);
        size_t actual_size = nvs.getBytes("registers", current_config.active_registers, reg_size);
        
        if (actual_size != reg_size || current_config.register_count == 0) {
            // Invalid data, use defaults
            set_default_config();
        }
        
        current_config.config_valid = true;
        xSemaphoreGive(config_mutex);
        return true;
    } else {
        Serial.println(F("[CONFIG] ERROR: Semaphore timeout in load_from_flash"));
        return false;
    }
    
    return false;
}

bool ConfigManager::save_to_flash() {
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        bool result = save_to_flash_unlocked();
        xSemaphoreGive(config_mutex);
        return result;
    } else {
        Serial.println(F("[CONFIG] ERROR: Semaphore timeout in save_to_flash"));
        return false;
    }
}

bool ConfigManager::save_to_flash_unlocked() {
    // This version assumes the mutex is already held
    nvs.putUInt("sampling_ms", current_config.sampling_interval_ms);
    nvs.putUInt("upload_ms", current_config.upload_interval_ms);
    nvs.putUChar("slave_addr", current_config.slave_address);
    nvs.putUChar("reg_count", current_config.register_count);
    nvs.putBytes("registers", current_config.active_registers, sizeof(current_config.active_registers));
    
    return true;
}

// Legacy apply_update method removed - configuration now handled through cloud integration

runtime_config_t ConfigManager::get_current_config() {
    runtime_config_t config;
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        config = current_config;
        xSemaphoreGive(config_mutex);
    }
    return config;
}

uint32_t ConfigManager::get_sampling_interval_ms() {
    uint32_t interval = POLL_INTERVAL_MS;
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        interval = current_config.sampling_interval_ms;
        xSemaphoreGive(config_mutex);
    }
    return interval;
}

uint32_t ConfigManager::get_upload_interval_ms() {
    uint32_t interval = UPLOAD_INTERVAL_MS;
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        interval = current_config.upload_interval_ms;
        xSemaphoreGive(config_mutex);
    }
    return interval;
}

uint8_t ConfigManager::get_slave_address() {
    uint8_t addr = SLAVE_ADDRESS;
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        addr = current_config.slave_address;
        xSemaphoreGive(config_mutex);
    }
    return addr;
}

uint8_t ConfigManager::get_register_count() {
    uint8_t count = READ_REGISTER_COUNT;
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        count = current_config.register_count;
        xSemaphoreGive(config_mutex);
    }
    return count;
}

void ConfigManager::get_active_registers(uint16_t* registers, uint8_t max_count) {
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        uint8_t count = min(current_config.register_count, max_count);
        for (uint8_t i = 0; i < count; i++) {
            registers[i] = current_config.active_registers[i];
        }
        xSemaphoreGive(config_mutex);
    }
}

bool ConfigManager::is_initialized() {
    return initialized;
}

bool ConfigManager::validate_sampling_interval(uint32_t interval_ms) {
    return (interval_ms >= limits.min_sampling_ms && interval_ms <= limits.max_sampling_ms);
}

bool ConfigManager::validate_upload_interval(uint32_t interval_ms) {
    return (interval_ms >= limits.min_upload_ms && interval_ms <= limits.max_upload_ms);
}

bool ConfigManager::validate_slave_address(uint8_t addr) {
    return (addr >= 1 && addr <= 247);
}

bool ConfigManager::validate_registers(const JsonArray& registers) {
    if (registers.size() == 0 || registers.size() > limits.max_register_count) {
        return false;
    }
    
    for (JsonVariant reg : registers) {
        String reg_name = reg.as<String>();
        if (get_register_address(reg_name) == 0xFFFF) {
            return false;
        }
    }
    
    return true;
}

uint16_t ConfigManager::get_register_address(const String& name) {
    for (size_t i = 0; i < REGISTER_MAP_SIZE; i++) {
        if (name.equals(REGISTER_MAP[i].name)) {
            return REGISTER_MAP[i].address;
        }
    }
    return 0xFFFF; // Invalid
}

bool ConfigManager::has_pending_changes() {
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        bool pending = has_pending_config;
        xSemaphoreGive(config_mutex);
        return pending;
    }
    return false;
}

void ConfigManager::apply_pending_config() {
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        if (has_pending_config) {
            Serial.println(F("[CONFIG] Applying pending configuration changes"));
            current_config = pending_config;
            has_pending_config = false;
            
            // Save to flash using unlocked version (we already have the mutex)
            if (save_to_flash_unlocked()) {
                Serial.println(F("[CONFIG] Pending configuration applied and saved to NVS"));
            } else {
                Serial.println(F("[CONFIG] Warning: Failed to save applied configuration to NVS"));
            }
        }
        xSemaphoreGive(config_mutex);
    } else {
        Serial.println(F("[CONFIG] ERROR: Semaphore timeout in apply_pending_config"));
    }
}

void ConfigManager::clear_pending_config() {
    if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) == pdTRUE) {
        has_pending_config = false;
        Serial.println(F("[CONFIG] Pending configuration cleared"));
        xSemaphoreGive(config_mutex);
    }
}

String ConfigManager::process_cloud_config_update(const String& response) {
    // Parse the cloud response to extract config_update
    String config_update_json;
    extern bool parse_config_update_from_response(const String& response, String& config_update_json);
    
    if (parse_config_update_from_response(response, config_update_json)) {
        Serial.println(F("[CONFIG] Processing configuration update from cloud response"));
        
        // Apply the configuration update and capture the ACK
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, config_update_json);
        
        if (error) {
            Serial.print(F("[CONFIG] JSON parse error: "));
            Serial.println(error.c_str());
            return "";
        }
        
        if (!doc["config_update"].is<JsonObject>()) {
            return "";
        }
        
        JsonObject config_update = doc["config_update"];
        JsonDocument ack_doc;
        JsonArray accepted = ack_doc["accepted"].to<JsonArray>();
        JsonArray rejected = ack_doc["rejected"].to<JsonArray>();
        JsonArray unchanged = ack_doc["unchanged"].to<JsonArray>();
        
        bool config_changed = false;
        
        if (xSemaphoreTake(config_mutex, CONFIG_MUTEX_TIMEOUT) != pdTRUE) {
            Serial.println(F("[CONFIG] ERROR: Semaphore timeout in process_cloud_config_update"));
            return "";
        }
        
        // Start with current config as the base for pending config
        pending_config = current_config;
        
        // Process each configuration parameter
        if (config_update["sampling_interval"].is<uint32_t>()) {
            uint32_t new_interval = config_update["sampling_interval"].as<uint32_t>() * 1000;
            if (!validate_sampling_interval(new_interval)) {
                rejected.add("sampling_interval");
            } else if (current_config.sampling_interval_ms == new_interval) {
                unchanged.add("sampling_interval");
            } else {
                pending_config.sampling_interval_ms = new_interval;
                accepted.add("sampling_interval");
                config_changed = true;
            }
        }
        
        if (config_update["upload_interval"].is<uint32_t>()) {
            uint32_t new_interval = config_update["upload_interval"].as<uint32_t>() * 1000;
            if (!validate_upload_interval(new_interval)) {
                rejected.add("upload_interval");
            } else if (current_config.upload_interval_ms == new_interval) {
                unchanged.add("upload_interval");
            } else {
                pending_config.upload_interval_ms = new_interval;
                accepted.add("upload_interval");
                config_changed = true;
            }
        }
        
        if (config_update["registers"].is<JsonArray>()) {
            JsonArray registers = config_update["registers"];
            if (!validate_registers(registers)) {
                rejected.add("registers");
            } else {
                bool registers_unchanged = (registers.size() == current_config.register_count);
                if (registers_unchanged) {
                    for (size_t i = 0; i < registers.size(); i++) {
                        String reg_name = registers[i].as<String>();
                        uint16_t addr = get_register_address(reg_name);
                        if (addr != current_config.active_registers[i]) {
                            registers_unchanged = false;
                            break;
                        }
                    }
                }
                
                if (registers_unchanged) {
                    unchanged.add("registers");
                } else {
                    pending_config.register_count = registers.size();
                    for (size_t i = 0; i < registers.size(); i++) {
                        String reg_name = registers[i].as<String>();
                        pending_config.active_registers[i] = get_register_address(reg_name);
                    }
                    accepted.add("registers");
                    config_changed = true;
                }
            }
        }
        
        if (config_update["slave_address"].is<uint8_t>()) {
            uint8_t new_addr = config_update["slave_address"].as<uint8_t>();
            if (!validate_slave_address(new_addr)) {
                rejected.add("slave_address");
            } else if (current_config.slave_address == new_addr) {
                unchanged.add("slave_address");
            } else {
                pending_config.slave_address = new_addr;
                accepted.add("slave_address");
                config_changed = true;
            }
        }
        
        if (config_changed) {
            has_pending_config = true;
            Serial.println(F("[CONFIG] Configuration changes staged as pending"));
        }
        
        xSemaphoreGive(config_mutex);
        
        // Generate and return the acknowledgment
        return generate_config_ack(accepted, rejected, unchanged);
    }
    
    return "";  // No config update found
}

String ConfigManager::generate_config_ack(const JsonArray& accepted, const JsonArray& rejected, const JsonArray& unchanged) {
    JsonDocument doc;
    JsonObject config_ack = doc["config_ack"].to<JsonObject>();
    
    config_ack["accepted"] = accepted;
    config_ack["rejected"] = rejected;
    config_ack["unchanged"] = unchanged;
    
    String result;
    serializeJson(doc, result);
    return result;
}

// Global functions
bool config_manager_init() {
    if (g_config_manager == nullptr) {
        g_config_manager = new ConfigManager();
    }
    return g_config_manager->init();
}

runtime_config_t config_get_current() {
    if (g_config_manager) {
        return g_config_manager->get_current_config();
    }
    
    // Return default config
    runtime_config_t default_config = {0};
    default_config.sampling_interval_ms = POLL_INTERVAL_MS;
    default_config.upload_interval_ms = UPLOAD_INTERVAL_MS;
    default_config.slave_address = SLAVE_ADDRESS;
    default_config.register_count = READ_REGISTER_COUNT;
    default_config.config_valid = false;
    return default_config;
}

uint32_t config_get_sampling_interval_ms() {
    if (g_config_manager) {
        return g_config_manager->get_sampling_interval_ms();
    }
    return POLL_INTERVAL_MS;
}

uint32_t config_get_upload_interval_ms() {
    if (g_config_manager) {
        return g_config_manager->get_upload_interval_ms();
    }
    return UPLOAD_INTERVAL_MS;
}

uint8_t config_get_slave_address() {
    if (g_config_manager) {
        return g_config_manager->get_slave_address();
    }
    return SLAVE_ADDRESS;
}

uint8_t config_get_register_count() {
    if (g_config_manager) {
        return g_config_manager->get_register_count();
    }
    return READ_REGISTER_COUNT;
}

void config_get_active_registers(uint16_t* registers, uint8_t max_count) {
    if (g_config_manager) {
        g_config_manager->get_active_registers(registers, max_count);
    } else {
        // Use default registers
        extern const PROGMEM uint16_t READ_REGISTERS[READ_REGISTER_COUNT];
        uint8_t count = min((uint8_t)READ_REGISTER_COUNT, max_count);
        for (uint8_t i = 0; i < count; i++) {
            registers[i] = pgm_read_word(&READ_REGISTERS[i]);
        }
    }
}

// Legacy config_apply_update function removed - configuration now handled through cloud integration

String config_process_cloud_response(const String& response) {
    if (g_config_manager) {
        return g_config_manager->process_cloud_config_update(response);
    }
    return "";
}

bool config_has_pending_changes() {
    if (g_config_manager) {
        return g_config_manager->has_pending_changes();
    }
    return false;
}

void config_apply_pending_changes() {
    if (g_config_manager) {
        g_config_manager->apply_pending_config();
    }
}

void config_clear_pending_changes() {
    if (g_config_manager) {
        g_config_manager->clear_pending_config();
    }
}