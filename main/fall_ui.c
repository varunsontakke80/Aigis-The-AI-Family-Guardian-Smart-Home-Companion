
/*
 * Fall UI: full-screen image alert for fall detection.
 *
 * Uses PNG bytes from `gui/image/Falldetected.h`.
 */

#include "fall_ui.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"

/* The provided header comes from image_to_c and contains full BMP files (assumed). */
#ifndef PROGMEM
#define PROGMEM
#endif

#include "gui/image/Falldetected.h"

static const char *TAG = "fall_ui";

static lv_obj_t *s_img = NULL;

/* Runtime-converted LVGL images (RGB565) */
static lv_img_dsc_t s_img_fall_dsc;
static uint8_t *s_buf_fall = NULL;

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
        // ESP_LOGW(TAG, "BMP truncated: needed=%lu len=%lu", (unsigned long)needed, (unsigned long)len);
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

    ESP_LOGI(TAG, "Initializing Fall UI images");

    esp_err_t err;
    
    err = bmp_to_lv_img(falldetected_img, sizeof(falldetected_img), &s_img_fall_dsc, &s_buf_fall);
    if (err != ESP_OK) { 
        ESP_LOGE(TAG, "Failed to load falldetected img"); 
        return err; 
    }

    s_imgs_ready = true;
    return ESP_OK;
}


static void apply_img(void)
{
    if (!s_img) {
        return;
    }
    
    /* If images aren't ready (e.g. placeholder), we can't show them. */
    if (ensure_images_ready() != ESP_OK) {
        ESP_LOGW(TAG, "apply_img: images not valid, cannot update UI");
        return;
    }
    
    lv_img_set_src(s_img, &s_img_fall_dsc);
}

esp_err_t fall_ui_start(void)
{
    ESP_LOGI(TAG, "fall_ui_start");
    bsp_display_lock(0);

    lv_obj_t *scr = lv_scr_act();
    
    /* Prepare images immediately */
    if (ensure_images_ready() != ESP_OK) {
        ESP_LOGI(TAG, "Failed to prepare images (might fail if no SPIRAM or bad BMP)");
    }

    s_img = lv_img_create(scr);
    lv_obj_clear_flag(s_img, LV_OBJ_FLAG_SCROLLABLE);
    /* Initially hidden until we explicitly show it via command */
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN); 
    
    /* Full screen */
    lv_obj_set_size(s_img, 320, 240);
    lv_obj_align(s_img, LV_ALIGN_CENTER, 0, 0);

    // Initial load
    apply_img();

    bsp_display_unlock();
    return ESP_OK;
}

void fall_ui_show(bool visible)
{
    if (!s_img) return;
    
    bsp_display_lock(0);
    s_is_visible = visible;
    if (visible) {
        lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
        /* Make sure it's on top */
        lv_obj_move_foreground(s_img);
        apply_img();
    } else {
        lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
    ESP_LOGI(TAG, "fall_ui_show(%d)", visible);
}

bool fall_ui_is_active(void)
{
    return s_is_visible;
}
