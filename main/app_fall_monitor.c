
/*
 * App Fall Monitor: Logic for handling fall alerts and health data.
 * Network reception is now handled by app_espnow.c.
 */

#include "app_fall_monitor.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "fall_ui.h"
#include "app/app_audio.h"

static const char *TAG = "fall_monitor";

// --- AUDIO CONFIG ---
// Path to the siren file. Verify checking spiffs content.
static const char *ALARM_AUDIO_PATH = "/spiffs/mp3/siren.mp3";

// Simple state
static bool s_alarm_active = false;

// Process received data (Called from app_espnow.c)
void app_fall_monitor_process_data(health_node_data_t *data) {
    if (data->fallDetected) {
        ESP_LOGE(TAG, "!!! FALL DETECTED RECEIVED !!!");
        
        if (!s_alarm_active) {
            s_alarm_active = true;
            
            // Show RED UI
            fall_ui_show(true);
            
            // Play Siren
            ESP_LOGI(TAG, "Attempting to play: %s", ALARM_AUDIO_PATH);
            
            FILE *fp = fopen(ALARM_AUDIO_PATH, "rb");
            if (fp) {
                fclose(fp);
                ESP_LOGI(TAG, "File exists on SPIFFS");
                esp_err_t audio_err = app_audio_play(ALARM_AUDIO_PATH);
                if (audio_err != ESP_OK) {
                    ESP_LOGE(TAG, "Audio play failed: %s", esp_err_to_name(audio_err));
                } else {
                    ESP_LOGI(TAG, "Audio play started successfully");
                }
            } else {
                ESP_LOGE(TAG, "File NOT found on SPIFFS: %s", ALARM_AUDIO_PATH);
            }
            
        } else {
             // Already active. 
        }
    }
    
    // Process Heart Rate / SPO2 if needed for UI update
    if (data->heartRate > 0 || data->spo2 > 0) {
        // TODO: Update Health UI if active
        // ESP_LOGI(TAG, "Health Data: HR=%.1f Spo2=%d", data->heartRate, data->spo2);
    }
}

esp_err_t app_fall_monitor_init(void)
{
    ESP_LOGI(TAG, "Initializing Fall Monitor Logic (UI/Audio ready)");
    // Network initialization is done in app_espnow_init()
    return ESP_OK;
}

void app_fall_monitor_stop_alarm(void)
{
    if (s_alarm_active) {
        ESP_LOGI(TAG, "Stopping Fall Alarm");
        s_alarm_active = false;
        
        // Stop Audio
        app_audio_stop();
        
        // Hide UI
        fall_ui_show(false);
    }
}
