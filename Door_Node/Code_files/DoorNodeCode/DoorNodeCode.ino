/* Includes ---------------------------------------------------------------- */
#include <Aigis_Door_Node_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

// --- Networking & ESP-NOW ---
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#if defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#else
#error "Camera model not selected"
#endif

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

// --- Hardware & Logic Defines ---
// HARDWARE FIX: Changed pins to avoid PSRAM and Bootstrapper conflicts
#define PIR_PIN 13                 // PIR sensor input 
#define RELAY_PIN 14               // Relay control (LOW = Lock, HIGH = Unlock)
#define FLASH_LED_PIN 4            // Onboard High-Power Flash LED
#define CONFIDENCE_THRESHOLD 0.60  // 70% confidence required to trigger

// --- ESP-NOW Configuration ---
// MAC ADDRESS OF ESP32-S3-BOX-3
uint8_t box3MacAddress[] = {0xB4, 0x3A, 0x45, 0xF3, 0x9E, 0x50};

// Structure to SEND data (Recognized Name)
typedef struct struct_message_send {
    char name[32];
} struct_message_send;

// Structure to RECEIVE data (Door Command)
typedef struct struct_message_recv {
    char command[16]; 
} struct_message_recv;

struct_message_send sendData;
struct_message_recv recvData;
esp_now_peer_info_t peerInfo;

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG, 
    .frame_size = FRAMESIZE_QVGA,    
    .jpeg_quality = 12, 
    .fb_count = 1,       
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

// --- ESP-NOW Callbacks ---

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Data Delivery Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

void OnDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len) {
    int copyLen = len;
    if (copyLen > sizeof(recvData)) {
        copyLen = sizeof(recvData);
    }
    
    memset(&recvData, 0, sizeof(recvData));   
    memcpy(&recvData, incomingData, copyLen); 

    Serial.printf("\n[Command Received]: %s\n", recvData.command);

    if (strcmp(recvData.command, "unlock") == 0) {
        digitalWrite(RELAY_PIN, HIGH); 
        Serial.println("Action: Door Unlocked!");
    }
    else if (strcmp(recvData.command, "lock") == 0) {
        digitalWrite(RELAY_PIN, LOW); 
        Serial.println("Action: Door Locked!");
    }
}

/**
* @brief      Arduino setup function
*/
void setup()
{
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Starting Node Initialization...");

    // Initialize Hardware Pins
    pinMode(PIR_PIN, INPUT_PULLDOWN); 
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); 
    digitalWrite(FLASH_LED_PIN, LOW); // Ensure flash is off at boot

    // Cleanly initialize Wi-Fi to Channel 11
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Prevents ESP-NOW packets dropping
    esp_wifi_start();
    esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Critical Error: Failed to initialize ESP-NOW");
        return;
    }

    // Register Callbacks
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // Register S3 Box 3 as a Peer
    memset(&peerInfo, 0, sizeof(peerInfo)); 
    memcpy(peerInfo.peer_addr, box3MacAddress, 6);
    peerInfo.channel = 11;  
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA; 

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add S3 Box 3 as peer");
        return;
    }

    if (ei_camera_init() == false) {
        ei_printf("Failed to initialize Camera!\r\n");
    } else {
        ei_printf("Camera initialized successfully.\r\n");
    }

    ei_printf("\nSystem Ready. Waiting for motion on PIR sensor...\n");
}

/**
* @brief      Main Loop - Awaits PIR trigger, runs inference, and transmits matches
*/
void loop()
{
    if (digitalRead(PIR_PIN) == HIGH) {
        Serial.println("\nMotion Detected! Capturing frame...");

        snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
        if(snapshot_buf == nullptr) {
            ei_printf("ERR: Failed to allocate snapshot buffer!\n");
            return;
        }

        ei::signal_t signal;
        signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
        signal.get_data = &ei_camera_get_data;

        // Turn on the Flash LED and wait slightly for auto-exposure to adjust
        digitalWrite(FLASH_LED_PIN, HIGH);
        delay(250);

        if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
            ei_printf("Failed to capture image\r\n");
            digitalWrite(FLASH_LED_PIN, LOW); // Turn off if capture fails
            free(snapshot_buf);
            return;
        }

        // Immediately turn off the Flash LED after the capture
        digitalWrite(FLASH_LED_PIN, LOW);

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
        if (err != EI_IMPULSE_OK) {
            ei_printf("ERR: Failed to run classifier (%d)\n", err);
            free(snapshot_buf);
            return;
        }

        char best_label[32] = "unknown";
        float best_value = 0.0;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
        for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
            ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
            if (bb.value > best_value) {
                best_value = bb.value;
                strcpy(best_label, bb.label);
            }
        }
#else
        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > best_value) {
                best_value = result.classification[i].value;
                strcpy(best_label, ei_classifier_inferencing_categories[i]);
            }
        }
#endif

        ei_printf("Top Prediction: %s (Confidence: %.2f)\n", best_label, best_value);

        if (best_value > CONFIDENCE_THRESHOLD) {
            if (strcmp(best_label, "Elon") == 0 || 
                strcmp(best_label, "Jack") == 0 || 
                strcmp(best_label, "Bill") == 0) {
                
                Serial.printf("Authorized Person Identified: %s. Sending to S3 Box 3...\n", best_label);
                
                strcpy(sendData.name, best_label);
                esp_now_send(box3MacAddress, (uint8_t *) &sendData, sizeof(sendData));
                
                delay(5000); 
            } else {
                Serial.println("Person not recognized as authorized.");
                delay(2000); 
            }
        } else {
            Serial.println("Confidence too low. Ignoring.");
            delay(1000);
        }

        free(snapshot_buf);
        
        while(digitalRead(PIR_PIN) == HIGH) {
            delay(100);
        }
        Serial.println("Motion cleared. Returning to standby.");
    } 
    else {
        delay(100);
    }
}

// =========================================================================
// Below are the unchanged helper functions provided from the original code
// =========================================================================

bool ei_camera_init(void) {
    if (is_initialised) return true;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1); 
      s->set_brightness(s, 1); 
      s->set_saturation(s, 0); 
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#elif defined(CAMERA_MODEL_ESP_EYE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_awb_gain(s, 1);
#endif

    is_initialised = true;
    return true;
}

void ei_camera_deinit(void) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ei_printf("Camera deinit failed\n");
        return;
    }
    is_initialised = false;
    return;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    return 0;
}