#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>

// Error codes
typedef enum {
    ERROR_NONE = 0,
    ERROR_WIFI_DISCONNECTED,
    ERROR_HTTP_TIMEOUT,
    ERROR_HTTP_FAILED,
    ERROR_INVALID_RESPONSE,
    ERROR_CRC_FAILED,
    ERROR_MODBUS_EXCEPTION,
    ERROR_INVALID_REGISTER,
    ERROR_MAX_RETRIES_EXCEEDED,
    ERROR_INVALID_HTTP_METHOD,
    ERROR_COMPRESSION_FAILED

} error_code_t;

// Error handling functions
void error_handler_init(void);
void log_error(error_code_t error_code, const char* message);
bool should_retry(error_code_t error_code, int retry_count);
unsigned long get_retry_delay(int retry_count);
void reset_error_state(void);
bool is_critical_error(error_code_t error_code);

// System health functions
void check_system_health(void);
bool handle_wifi_reconnection(void);
void feed_watchdog(void);

#endif
