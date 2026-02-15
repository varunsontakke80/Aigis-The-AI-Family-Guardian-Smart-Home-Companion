#pragma once

#include "esp_err.h"
#include "app/app_espnow.h" // For health_node_data_t

/**
 * @brief Initialize Fall Monitor (UI/Audio resources only).
 *        Network init is handled by app_espnow.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_fall_monitor_init(void);

/**
 * @brief Process received data from Health Node.
 * 
 * @param data Pointer to the received health data structure.
 */
void app_fall_monitor_process_data(health_node_data_t *data);

/**
 * @brief Stop the active fall alarm (audio + UI).
 */
void app_fall_monitor_stop_alarm(void);
