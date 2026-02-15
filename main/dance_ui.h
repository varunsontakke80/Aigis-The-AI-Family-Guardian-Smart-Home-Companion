#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the Dance UI module.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t dance_ui_start(void);

/**
 * @brief Show or hide the Dance UI.
 * 
 * @param visible true to show the dancing image, false to hide it.
 */
void dance_ui_show(bool visible);

/**
 * @brief Check if Dance UI is currently active/visible.
 * 
 * @return true if visible, false otherwise.
 */
bool dance_ui_is_active(void);
