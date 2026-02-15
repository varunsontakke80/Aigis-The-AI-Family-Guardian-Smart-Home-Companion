/**
 * @file app_uart.h
 * @brief UART communication with Arduino Nano
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART for communication with Arduino Nano
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_uart_init(void);

/**
 * @brief Send a single character command to Arduino Nano
 * 
 * @param cmd The character command to send (e.g., 'F', 'B', 'D', 'H', 'J')
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_uart_send_cmd(char cmd);

#ifdef __cplusplus
}
#endif
