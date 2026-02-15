/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_storage.h"
#include "bsp_board.h"
#include "bsp/esp-bsp.h"

#include "app_sr.h"
#include "main_ui.h"
#include "gui/ui_boot_animate.h"
#include "gui/ui_sr.h"

#include "door_ui.h"
#include "dance_ui.h"
#include "story_ui.h"
#include "fall_ui.h"
#include "app_fall_monitor.h"
#include "app/app_audio.h"
#include "app/app_uart.h"
#include "app/app_espnow.h"

static const char *TAG = "main";

static void after_boot(void)
{
    /* Minimal main screen + SR overlay */
    ESP_ERROR_CHECK(main_ui_start());
    ESP_ERROR_CHECK(door_ui_start());
    ESP_ERROR_CHECK(dance_ui_start());
    ESP_ERROR_CHECK(story_ui_start());
    ESP_ERROR_CHECK(fall_ui_start());
    ui_sr_anim_init();
    
    ESP_ERROR_CHECK(app_audio_start());

}


void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* UART for Nano */
    ESP_ERROR_CHECK(app_uart_init());
    
    /* Centralized ESP-NOW Init (Network + Peers) */
    ESP_ERROR_CHECK(app_espnow_init());



    /* Needed for SR echo wav files */
    bsp_spiffs_mount();

    bsp_i2c_init();

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    cfg.lvgl_port_cfg.task_affinity = 1;
    bsp_display_start_with_config(&cfg);
    bsp_board_init();

    ESP_LOGI(TAG, "start main UI");
    bsp_display_lock(0);
    boot_animate_start(after_boot);
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "speech recognition start (english, 2 cmds)");
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_ERROR_CHECK(app_sr_start(false));
    ESP_ERROR_CHECK(app_fall_monitor_init());


}
