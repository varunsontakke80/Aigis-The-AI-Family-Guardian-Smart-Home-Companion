/*
 * Door UI: full-screen image button that toggles the door lock state (visually).
 *
 * Uses PNG bytes from `gui/image/lock.h` and `gui/image/unlock.h`.
 */

#include "door_ui.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "bsp/esp-bsp.h"
#include "lvgl.h"

/* The provided headers come from image_to_c and contain full BMP files (assumed). */
#ifndef PROGMEM
#define PROGMEM
#endif

#include "gui/image/lock.h"
#include "gui/image/unlock.h"

static const char *TAG = "door_ui";

static lv_obj_t *s_img = NULL;

/* Runtime-converted LVGL images (RGB565) */
static lv_img_dsc_t s_img_lock_dsc;
static lv_img_dsc_t s_img_unlock_dsc;

static uint8_t *s_buf_lock = NULL;
static uint8_t *s_buf_unlock = NULL;

static bool s_imgs_ready = false;
static bool s_is_visible = false;
static bool s_current_state = false; // false = unlocked, true = locked (default)

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
    // ESP_LOGI(TAG, "Allocating %u bytes for image. Free SPIRAM: %u", (unsigned)out_size, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
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

    ESP_LOGI(TAG, "Initializing Door UI images");

    /* Lock Image */
    /* Note: We use an enormous size cap for checks because the array size isn't known 
       via sizeof if it's extern, but here it's included so sizeof works usually, 
       UNLESS it's a pointer. The header usually has `const uint8_t lock_img[] = ...`
       So `sizeof(lock_img)` should work.
    */
    esp_err_t err;
    
    err = bmp_to_lv_img(lock_img, sizeof(lock_img), &s_img_lock_dsc, &s_buf_lock);
    if (err != ESP_OK) { 
        ESP_LOGE(TAG, "Failed to load lock img"); 
        return err; 
    }

    err = bmp_to_lv_img(unlock_img, sizeof(unlock_img), &s_img_unlock_dsc, &s_buf_unlock);
    if (err != ESP_OK) { 
        ESP_LOGE(TAG, "Failed to load unlock img"); 
        return err; 
    }

    s_imgs_ready = true;
    return ESP_OK;
}


static lv_obj_t *s_label = NULL;

static void apply_img(bool locked)
{
    if (!s_img) {
        return;
    }
    
    /* If images aren't ready (e.g. placeholder), we can't show them. */
    if (ensure_images_ready() != ESP_OK) {
        ESP_LOGW(TAG, "apply_img: images not valid, cannot update UI");
        return;
    }
    
    ESP_LOGI(TAG, "apply_img: state=%s", locked ? "LOCKED" : "UNLOCKED");
    lv_img_set_src(s_img, locked ? &s_img_lock_dsc : &s_img_unlock_dsc);
    
    // Hide label when showing lock/unlock state
    if (s_label) {
        lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    }
}

esp_err_t door_ui_start(void)
{
    ESP_LOGI(TAG, "door_ui_start");
    bsp_display_lock(0);

    lv_obj_t *scr = lv_scr_act();
    
    /* Prepare images immediately */
    if (ensure_images_ready() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare images");
    }

    s_img = lv_img_create(scr);
    lv_obj_clear_flag(s_img, LV_OBJ_FLAG_SCROLLABLE);
    /* Initially hidden until we explicitly show it via command */
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN); 
    
    /* Full screen */
    lv_obj_set_size(s_img, 320, 240);
    lv_obj_align(s_img, LV_ALIGN_CENTER, 0, 0);

    // Initial state
    apply_img(s_current_state);
    
    // Create Label for Person Name
    s_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_24, 0); // Large font
    lv_obj_align(s_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);

    bsp_display_unlock();
    return ESP_OK;
}

void door_ui_set(bool locked)
{
    bsp_display_lock(0);
    s_current_state = locked;
    apply_img(locked);
    bsp_display_unlock();
}

void door_ui_show(bool visible)
{
    if (!s_img) return;
    
    bsp_display_lock(0);
    s_is_visible = visible;
    if (visible) {
        lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
        /* Make sure it's on top if needed */
        lv_obj_move_foreground(s_img);
        apply_img(s_current_state);
    } else {
        lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
        if (s_label) lv_obj_add_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
    ESP_LOGI(TAG, "door_ui_show(%d)", visible);
}

void door_ui_show_person(const char *name)
{
    if (!s_img || !s_label) return;

    bsp_display_lock(0);
    
    // Ensure resources
    if (ensure_images_ready() != ESP_OK) {
        bsp_display_unlock();
        return;
    }

    // 1. Show Image (Placeholder: using Lock Image for now as requested)
    // TODO: Replace with someone_at_door_img when header is available
    lv_img_set_src(s_img, &s_img_lock_dsc); 
    lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_img);

    // 2. Show Name Label
    lv_label_set_text_fmt(s_label, "Person: %s", name);
    lv_obj_clear_flag(s_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_label);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Showing Door UI with person: %s", name);
}
