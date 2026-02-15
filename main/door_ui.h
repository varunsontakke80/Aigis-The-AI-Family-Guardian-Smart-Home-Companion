#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Start the Door UI (loads resources)
 */
esp_err_t door_ui_start(void);

/**
 * @brief Update the visual state (Locked/Unlocked)
 */
void door_ui_set(bool locked);

/**
 * @brief Show or hide the Door UI
 */
void door_ui_show(bool visible);

/**
 * @brief Show the detected person on the UI
 * 
 * @param name Name of the person detected
 */
void door_ui_show_person(const char *name);
