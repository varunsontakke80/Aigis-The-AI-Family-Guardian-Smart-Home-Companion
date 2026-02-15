/*
 * Audio App: Wrapper for esp-audio-player to play MP3s from SPIFFS.
 */

#include "app_audio.h"
#include "audio_player.h" // From esp-audio-player component
#include "bsp_board.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdio.h>

static const char *TAG = "app_audio";

/* Audio Player Callback Wrapper */
static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        ESP_LOGI(TAG, "Audio IDLE");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
        ESP_LOGI(TAG, "Audio PLAYING NEXT");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        ESP_LOGI(TAG, "Audio PLAYING");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        ESP_LOGI(TAG, "Audio PAUSE");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
        ESP_LOGI(TAG, "Audio SHUTDOWN");
        break;
    default:
        break;
    }
}

/* Mute wrapper for audio player */
static esp_err_t audio_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    bool mute = (setting == AUDIO_PLAYER_MUTE);
    return bsp_codec_mute_set(mute);
}

/* Clock config wrapper for audio player */
static esp_err_t audio_clk_set_fn(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    return bsp_codec_set_fs(rate, bits_cfg, ch);
}

/* Write wrapper for audio player */
static esp_err_t audio_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    return bsp_i2s_write(audio_buffer, len, bytes_written, timeout_ms);
}

esp_err_t app_audio_start(void)
{
    audio_player_config_t config = {
        .mute_fn = audio_mute_fn,
        .clk_set_fn = audio_clk_set_fn,
        .write_fn = audio_write_fn,
        .priority = 5
    };

    ESP_ERROR_CHECK(audio_player_new(config));
    ESP_ERROR_CHECK(audio_player_callback_register(audio_callback, NULL));

    ESP_LOGI(TAG, "Audio Player Initialized");
    return ESP_OK;
}

esp_err_t app_audio_play(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    esp_err_t ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to play audio: %d", ret);
        fclose(fp); // audio_player_play closes fp on ESP_OK only
    }
    return ret;
}

esp_err_t app_audio_stop(void)
{
    return audio_player_stop();
}

esp_err_t app_audio_pause(void)
{
    return audio_player_pause();
}

esp_err_t app_audio_resume(void)
{
    return audio_player_resume();
}

esp_err_t app_audio_volume_set(int volume)
{
    int vol_set = 0;
    return bsp_codec_volume_set(volume, &vol_set);
}

