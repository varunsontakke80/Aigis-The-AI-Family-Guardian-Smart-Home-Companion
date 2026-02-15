/*
 * Story UI: full-screen image for "Tell a Story" command.
 *
 * Uses raw RGB565 bytes from `gui/image/story.h`.
 */

#include "story_ui.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "app_audio.h"

/* The provided header contains raw RGB565 data */
#ifndef PROGMEM
#define PROGMEM
#endif

#include "gui/image/story.h"

static const char *TAG = "story_ui";

static lv_obj_t *s_img = NULL;

/* Runtime-converted LVGL images (RGB565) */
static lv_img_dsc_t s_img_story_dsc;
static uint8_t *s_buf_story = NULL;

static bool s_imgs_ready = false;
static bool s_is_visible = false;


static esp_err_t bmp_to_lv_img(const uint8_t *bmp, size_t len, lv_img_dsc_t *out_dsc, uint8_t **out_buf)
{
    if (!bmp || len < 54) {
        ESP_LOGE(TAG, "Buffer too small");
        return ESP_ERR_INVALID_ARG;
    }

    /* parse BMP header */
    uint32_t data_offset = 0;
    int32_t width = 0;
    int32_t height = 0;
    
    if (bmp[0] == 0x42 && bmp[1] == 0x4d) {
        /* Standard BMP */
        data_offset = *(const uint32_t *)&bmp[10];
        width = *(const int32_t *)&bmp[18];
        height = *(const int32_t *)&bmp[22];
        uint16_t bpp = *(const uint16_t *)&bmp[28];
        uint32_t compression = *(const uint32_t *)&bmp[30];

        if (width <= 0 || height <= 0 || bpp != 24 || compression != 0) {
            ESP_LOGE(TAG, "Unsupported BMP: w=%ld h=%ld bpp=%u comp=%lu",
                     (long)width, (long)height, (unsigned)bpp, (unsigned long)compression);
            return ESP_ERR_NOT_SUPPORTED;
        }
        ESP_LOGI(TAG, "Detected BMP header. size=%dx%d", (int)width, (int)height);
    } else {
        /* Fallback: Assume raw BGR data for 320x240 */
        ESP_LOGW(TAG, "Detailed Header Check: Byte0=0x%02X Byte1=0x%02X. expected 0x42 0x4D. Assuming RAW 320x240 BGR", bmp[0], bmp[1]);
        width = 320;
        height = 240;
        data_offset = 0; // Assume start of array
    }

    uint32_t row_stride = ((width * 3) + 3) & ~3U; /* 4-byte aligned rows */
    uint32_t needed = data_offset + row_stride * (uint32_t)height;
    
    /* Allow loose size check for raw data as it might not be padded */
    if (needed > len && data_offset != 0) {
        ESP_LOGW(TAG, "BMP truncated: needed=%lu len=%lu", (unsigned long)needed, (unsigned long)len);
        /* If rigorous check fails, we can still try if it's raw-like */
    }

    size_t out_size = (size_t)width * (size_t)height * 2; /* RGB565 */
    
    uint8_t *buf = heap_caps_malloc(out_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "No mem for RGB565 buffer (%lu bytes). Free SPIRAM: %u", 
                 (unsigned long)out_size, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        return ESP_ERR_NO_MEM;
    }

    const uint8_t *src_base = bmp + data_offset;

    /* BMP is bottom-up usually, but raw exports might be top-down. 
       Standard BMP is bottom-up. 
       If header was missing, it's likely pixel data. 
       Let's assume top-down for raw, bottom-up for BMP. */
    
    bool bottom_up = (bmp[0] == 0x42 && bmp[1] == 0x4d);

    for (int y = 0; y < height; y++) {
        int src_y = bottom_up ? (height - 1 - y) : y;
        /* Calculate row manually. If raw, stride is just width*3 usually */
        const uint8_t *src_row;
        if (bottom_up) {
             src_row = src_base + row_stride * src_y;
        } else {
             /* Raw data often packed exactly */
             src_row = src_base + (width * 3) * src_y;
             /* Safety check boundaries */
             if ((src_row - bmp) + width*3 > len) break; 
        }

        uint16_t *dst_row = (uint16_t *)(buf + (size_t)y * (size_t)width * 2);
        for (int x = 0; x < width; x++) {
            uint8_t b = src_row[x * 3 + 0];
            uint8_t g = src_row[x * 3 + 1];
            uint8_t r = src_row[x * 3 + 2];
            /* Let LVGL handle RGB565 layout / byte order */
            lv_color_t c = lv_color_make(r, g, b);
            dst_row[x] = c.full;
        }
    }

    out_dsc->header.always_zero = 0;
    out_dsc->header.w = (uint32_t)width;
    out_dsc->header.h = (uint32_t)height;
    out_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    out_dsc->data_size = out_size;
    out_dsc->data = buf;

    *out_buf = buf;
    return ESP_OK;
}

static esp_err_t ensure_images_ready(void)
{
    if (s_imgs_ready) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Story UI images");
    esp_err_t err = bmp_to_lv_img(story_img, sizeof(story_img), &s_img_story_dsc, &s_buf_story);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load story img");
        return err;
    }

    s_imgs_ready = true;
    return ESP_OK;
}

static void create_ui(void)
{
    if (s_img) return;

    ESP_LOGI(TAG, "Creating Story UI object");
    
    if (ensure_images_ready() != ESP_OK) {
        ESP_LOGE(TAG, "Cannot create Story UI, img load failed");
    }

    s_img = lv_img_create(lv_scr_act());
    lv_obj_set_size(s_img, 320, 240);
    lv_obj_center(s_img);
    // Hide initially
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    
    if (s_imgs_ready) {
        lv_img_set_src(s_img, &s_img_story_dsc);
    }
}

void story_ui_show(bool visible)
{
    if (!s_img) return;

    s_is_visible = visible;

    if (visible) {
        if (ensure_images_ready() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load story image");
            return;
        }
        lv_img_set_src(s_img, &s_img_story_dsc);
        lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Story UI Shown");
        /* Play the story audio */
        app_audio_volume_set(80);
        app_audio_play("/spiffs/mp3/Short_Story.mp3");
    } else {
        lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Story UI Hidden");
        /* Stop audio when hidden */
        app_audio_stop();
    }
}

esp_err_t story_ui_start(void)
{
    if (bsp_display_lock(0)) {
        create_ui();
        bsp_display_unlock();
        ESP_LOGI(TAG, "story_ui_start success");
    } else {
        ESP_LOGE(TAG, "Failed to lock display for story_ui_start");
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool story_ui_is_active(void)
{
    return s_is_visible;
}
