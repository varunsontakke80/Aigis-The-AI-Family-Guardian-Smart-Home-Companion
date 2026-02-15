#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize the Story UI (create LVGL objects, initially hidden)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t story_ui_start(void);

/**
 * @brief Show or hide the Story UI.
 *        When shown, it also starts playing the story audio.
 *        When hidden, it stops the audio.
 * 
 * @param visible true to show, false to hide.
 */
void story_ui_show(bool visible);

/**
 * @brief Check if Story UI is currently active/visible.
 * 
 * @return true if visible, false otherwise.
 */
bool story_ui_is_active(void);
