
#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the Fall UI (load images, create objects, hide them initially).
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t fall_ui_start(void);

/**
 * @brief Show or hide the Fall Detected alert.
 * 
 * @param visible true to show (and bring to front), false to hide.
 */
void fall_ui_show(bool visible);

/**
 * @brief Check if the Fall UI is currently visible.
 */
bool fall_ui_is_active(void);
