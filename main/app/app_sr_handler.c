/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"

#include "bsp_board.h"
#include "bsp/esp-bsp.h"

#include "app_sr.h"
#include "app_sr_handler.h"
#include "app_uart.h"
#include "app_espnow.h"

#include "ui_sr.h"
#include "main_ui.h"
#include "door_ui.h"
#include "dance_ui.h"
#include "story_ui.h"
#include "fall_ui.h"
#include "app_fall_monitor.h"
#include "app_audio.h"


static const char *TAG = "sr_handler";

static bool s_audio_playing = false;

typedef enum {
    AUDIO_WAKE,
    AUDIO_OK,
    AUDIO_END,
    AUDIO_MAX,
} audio_segment_t;

typedef struct {
    uint8_t *buf;
    size_t len;
} audio_data_t;

static audio_data_t s_audio[AUDIO_MAX];

static esp_err_t load_wav_to_mem(audio_segment_t seg, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    if (s_audio[seg].buf) {
        heap_caps_free(s_audio[seg].buf);
        s_audio[seg].buf = NULL;
        s_audio[seg].len = 0;
    }

    s_audio[seg].len = (size_t)sz;
    s_audio[seg].buf = heap_caps_malloc(s_audio[seg].len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_audio[seg].buf) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    fread(s_audio[seg].buf, 1, s_audio[seg].len, fp);
    fclose(fp);
    return ESP_OK;
}

static esp_err_t sr_echo_init(void)
{
    /* English only */
    ESP_LOGI(TAG, "Loading SR echo wavs from SPIFFS");
    ESP_RETURN_ON_ERROR(load_wav_to_mem(AUDIO_WAKE, "/spiffs/echo_en_wake.wav"), TAG, "load wake wav failed");
    ESP_RETURN_ON_ERROR(load_wav_to_mem(AUDIO_OK, "/spiffs/echo_en_ok.wav"), TAG, "load ok wav failed");
    ESP_RETURN_ON_ERROR(load_wav_to_mem(AUDIO_END, "/spiffs/echo_en_end.wav"), TAG, "load end wav failed");
    ESP_LOGI(TAG, "SR echo wavs loaded: wake=%u bytes, ok=%u bytes, end=%u bytes",
             (unsigned)s_audio[AUDIO_WAKE].len,
             (unsigned)s_audio[AUDIO_OK].len,
             (unsigned)s_audio[AUDIO_END].len);
    return ESP_OK;
}

static esp_err_t sr_echo_play(audio_segment_t seg)
{
    typedef struct {
        uint8_t ChunkID[4];
        int32_t ChunkSize;
        uint8_t Format[4];
        uint8_t Subchunk1ID[4];
        int32_t Subchunk1Size;
        int16_t AudioFormat;
        int16_t NumChannels;
        int32_t SampleRate;
        int32_t ByteRate;
        int16_t BlockAlign;
        int16_t BitsPerSample;
        uint8_t Subchunk2ID[4];
        int32_t Subchunk2Size;
    } wav_header_t;

    if (!s_audio[seg].buf || s_audio[seg].len <= sizeof(wav_header_t)) {
        ESP_LOGW(TAG, "sr_echo_play(%d): buffer invalid (len=%u)", seg, (unsigned)s_audio[seg].len);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t *p = s_audio[seg].buf;
    wav_header_t *h = (wav_header_t *)p;
    p += sizeof(wav_header_t);
    size_t len = s_audio[seg].len - sizeof(wav_header_t);
    len &= 0xfffffffc;

    /* Configure codec for wav format */
    ESP_LOGI(TAG, "sr_echo_play(%d): %d Hz, %d bits, data_len=%u", seg,
             (int)h->SampleRate, (int)h->BitsPerSample, (unsigned)len);
    bsp_codec_set_fs(h->SampleRate, h->BitsPerSample, I2S_SLOT_MODE_STEREO);
    /* Ensure codec is unmuted and volume is reasonable for feedback tones */
    bsp_codec_mute_set(false);
    int vol = 100;
    bsp_codec_volume_set(vol, &vol);

    size_t bytes_written = 0;
    s_audio_playing = true;
    bsp_i2s_write((char *)p, len, &bytes_written, portMAX_DELAY);
    ESP_LOGI(TAG, "sr_echo_play(%d): wrote %u bytes", seg, (unsigned)bytes_written);
    vTaskDelay(pdMS_TO_TICKS(20));
    s_audio_playing = false;
    return ESP_OK;
}

bool sr_echo_is_playing(void)
{
    return s_audio_playing;
}

void sr_handler_task(void *pvParam)
{
    (void)pvParam;

    /* Needs SPIFFS mounted in main */
    if (sr_echo_init() != ESP_OK) {
        ESP_LOGW(TAG, "SR echo wav init failed (tones disabled)");
    }

    while (true) {
        sr_result_t result;
        app_sr_get_result(&result, portMAX_DELAY);

        if (result.wakenet_mode == WAKENET_DETECTED) {
            sr_anim_start();
            sr_anim_set_text("Say command");
            (void)sr_echo_play(AUDIO_WAKE);
            continue;
        }

        if (result.state == ESP_MN_STATE_TIMEOUT) {
            if (dance_ui_is_active() || story_ui_is_active()) {
                ESP_LOGI(TAG, "Timeout suppressed due to active mode");
                sr_anim_stop();
                continue;
            }

            sr_anim_set_text("Timeout");
            (void)sr_echo_play(AUDIO_END);
            sr_anim_stop();
            continue;
        }

        if (ESP_MN_STATE_DETECTED & result.state) {
            const sr_cmd_t *cmd = app_sr_get_cmd_from_id(result.command_id);
            if (cmd) {
                ESP_LOGI(TAG, "command:%s, act:%d", cmd->str, cmd->cmd);
                sr_anim_set_text((char *)cmd->str);
            }
            (void)sr_echo_play(AUDIO_OK);
            sr_anim_stop();

            switch (cmd ? cmd->cmd : SR_CMD_MAX) {
            // Smart home Node  commands
            case SR_CMD_TURN_ON_LIGHT_ONE:
                // send light_one_ctrl_on command to smart home node
                app_espnow_send_command(TURN_ON_LIGHT_ONE);
                break;
            case SR_CMD_TURN_OFF_LIGHT_ONE:
                // send light_one_ctrl_off command to smart home node
                app_espnow_send_command(TURN_OFF_LIGHT_ONE);
                break;
            case SR_CMD_TURN_ON_SOCKET:
                // send socket_ctrl_on command to smart home node
                app_espnow_send_command(TURN_ON_SOCKET);
                break;
            case SR_CMD_TURN_OFF_SOCKET:
                // send socket_ctrl_off command to smart home node
                app_espnow_send_command(TURN_OFF_SOCKET);
                break;
            case SR_CMD_TURN_ON_FAN_AT_LEVEL_ONE:
                // send fan_ctrl_on_level_one command to smart home node
                app_espnow_send_command(TURN_ON_FAN_AT_LEVEL_ONE);
                break;
            case SR_CMD_TURN_ON_FAN_AT_LEVEL_TWO:
                // send fan_ctrl_on_level_two command to smart home node
                app_espnow_send_command(TURN_ON_FAN_AT_LEVEL_TWO);
                break;
            case SR_CMD_TURN_ON_FAN_AT_LEVEL_THREE:
                // send fan_ctrl_on_level_three command to smart home node
                app_espnow_send_command(TURN_ON_FAN_AT_LEVEL_THREE);
                break;
            case SR_CMD_TURN_OFF_FAN:
                // send fan_ctrl_off command to smart home node
                app_espnow_send_command(TURN_OFF_FAN);
                break;
            //Health Node command task
            case SR_CMD_CHECK_HEALTH:
                // read health_status command from Health node
                // health_ui_set(true) show result and text of data that recieved for heart and spo2
                break;
            // DOOR NODE commands task
            case SR_CMD_LOCK_THE_DOOR:
                // send lock_the_door command to Door node
                app_espnow_send_door_command("lock");
                // Switch to Door UI
                main_ui_show(false);
                door_ui_show(true);
                door_ui_set(true);
                // Wait for 3 seconds then return to Main UI
                vTaskDelay(pdMS_TO_TICKS(3000));
                door_ui_show(false);
                main_ui_show(true);
                break;
            case SR_CMD_UNLOCK_THE_DOOR:
                // send unlock_the_door command to Door node
                app_espnow_send_door_command("unlock");
                // Switch to Door UI
                main_ui_show(false);
                door_ui_show(true);
                door_ui_set(false);
                // Wait for 3 seconds then return to Main UI
                vTaskDelay(pdMS_TO_TICKS(3000));
                door_ui_show(false);
                main_ui_show(true);
                break;
            // robot task on command
            case SR_CMD_WALK_FORWARD_AIGIS:
                ESP_LOGI(TAG, "Walk Forward Aigis");
                app_uart_send_cmd('F'); // Send UART command
                // send walk command to robot node
                // walking_face_ui_set(true)
                break;
            case SR_CMD_STOP_AIGIS:
                ESP_LOGI(TAG, "Stop Aigis");
                sr_anim_set_text("Stopped");
                app_uart_send_cmd('H'); // Send UART command
                // Stop Dance UI and Audio
                dance_ui_show(false);
                // Stop Story UI and Audio
                story_ui_show(false);
                // Stop Fall Alarm
                app_fall_monitor_stop_alarm();
                // Return to default UI
                main_ui_show(true); 
                break;
            case SR_CMD_LETS_DANCE_AIGIS:
                ESP_LOGI(TAG, "Let's Dance Aigis");
                sr_anim_set_text("Dancing...");
                app_uart_send_cmd('D'); // Send UART command
                // Hide other UIs
                main_ui_show(false);
                door_ui_show(false);
                // Show Dance UI and Play Audio
                dance_ui_show(true);
                break;
            case SR_CMD_TELL_A_STORY:
                ESP_LOGI(TAG, "Tell a Story");
                sr_anim_set_text("Story Time");
                // Hide other UIs
                main_ui_show(false);
                door_ui_show(false);
                dance_ui_show(false);
                // Show Story UI and Play Audio
                story_ui_show(true);
                break;
            default:
                ESP_LOGW(TAG, "Unhandled cmd");
                break;
            }
            
            // For other generic commands that don't switch UI permanently, 
            // ensure we are back on Main Face if not in a persistent mode.
            // However, current logic in switch cases is specific. 
            // The user requested: "show Main_Face.h image in LCD after any comand is exicuted".
            // Since Dance and Story seem to be persistent modes (playing audio), we leave them active.
            // Door commands have the 3s delay implemented above.
            // Other commands (like Smart Home) are currently empty but should default to Main Face too if they had UI.
            // Since they are empty comments, no UI change happens, so Main Face stays if it was there.
        }
    }
}
