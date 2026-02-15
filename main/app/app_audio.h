#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the audio player (esp-audio-player).
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_audio_start(void);

/**
 * @brief Play an MP3 file from the given path.
 * 
 * @param path File path (e.g., "/spiffs/mp3/music.mp3")
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_audio_play(const char *path);

/**
 * @brief Stop audio playback.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_audio_stop(void);

/**
 * @brief Pause audio playback.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_audio_pause(void);

/**
 * @brief Resume audio playback.
 * 
 * @return esp_err_t ESP_OK on success.
 */

/**
 * @brief Set the audio volume.
 * 
 * @param volume Volume level (0-100).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t app_audio_volume_set(int volume);

#ifdef __cplusplus
}
#endif

