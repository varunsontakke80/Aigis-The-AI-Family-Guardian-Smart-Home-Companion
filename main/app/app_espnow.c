#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "app_espnow.h"
#include "app_espnow.h"
#include "app_fall_monitor.h"
#include "door_ui.h"
#include "app_audio.h"

static const char *TAG = "app_espnow";

// Remote Node MAC Address (Control Node - NodeMCU)
static uint8_t remote_mac_control[] = {0xE8, 0xDB, 0x84, 0x11, 0xEF, 0x14};

// Remote Node MAC Address (Health/Fall Node - Xiao C3)
static const uint8_t remote_mac_health[] = {0x94, 0xA9, 0x90, 0x69, 0x13, 0xF4};

// Remote Node MAC Address (Door Node - ESP32-CAM)
static const uint8_t remote_mac_door[] = {0x3C, 0x61, 0x05, 0x30, 0x78, 0xF0};

// Callback when data is sent
static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Last Packet Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback when data is received
static void OnDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len) {
    const uint8_t *mac = esp_now_info->src_addr;
    
    ESP_LOGI(TAG, "Packet recv from: %02x:%02x:%02x:%02x:%02x:%02x, len: %d", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);

    if (memcmp(mac, remote_mac_health, 6) == 0) {
        ESP_LOGI(TAG, "Packet is from Health Node");
        if (len == sizeof(health_node_data_t)) {
            health_node_data_t *data = (health_node_data_t *)incomingData;
            ESP_LOGI(TAG, "Health Data: Fall=%d, Alarm=%d, HR=%f, SpO2=%d", 
                     data->fallDetected, data->healthAlarm, data->heartRate, data->spo2);
            app_fall_monitor_process_data(data);
        } else {
            ESP_LOGW(TAG, "Health Node data len mismatch: %d != %d", len, sizeof(health_node_data_t));
        }
    } else if (memcmp(mac, remote_mac_control, 6) == 0) {
        ESP_LOGI(TAG, "Packet from Control Node (ignored)");
    } else if (memcmp(mac, remote_mac_door, 6) == 0) {
        ESP_LOGI(TAG, "Packet is from Door Node");
        if (len == sizeof(door_node_data_recv_t)) {
             door_node_data_recv_t *data = (door_node_data_recv_t *)incomingData;
             // Ensure null termination safely
             char name[33];
             strncpy(name, data->name, 32);
             name[32] = '\0';
             ESP_LOGI(TAG, "Person Detected: %s", name);
             door_ui_show_person(name);
             app_audio_play("/spiffs/mp3/Someone_at_door_voice.mp3");
        } else {
             ESP_LOGW(TAG, "Door Node data len mismatch: %d != %d", len, sizeof(door_node_data_recv_t));
        }
    } else {
        ESP_LOGW(TAG, "Unknown MAC");
    }
}

esp_err_t app_espnow_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    // NVS might be already inited by other components, so we don't strictly check ESP_ERR_INVALID_STATE
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "NVS Init failed: %s", esp_err_to_name(ret));
    }

    // Netif might be already inited
    esp_netif_init(); 
    
    // Event loop might be created by bsp_board_init or others
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event Loop Init failed: %s", esp_err_to_name(ret));
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
         ESP_LOGE(TAG, "WiFi Init failed: %s", esp_err_to_name(ret));
         return ret;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set Channel 11 to match Health Node
    ESP_ERROR_CHECK(esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE));

    // Log MAC Address for verification
    uint8_t mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac_addr));
    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    
    // Low Tx Power to avoid brownout on USB power
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        // May already be initialized
        ESP_LOGW(TAG, "ESP-NOW Init failed (or already info)");
    }
    
    // Register Send Callback
    esp_now_register_send_cb(OnDataSent);

    // Register Recv Callback
    esp_now_register_recv_cb(OnDataRecv);
    
    // Register Peer (Control Node)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, remote_mac_control, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    ret = esp_now_add_peer(&peerInfo);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST){
        ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register Peer (Health Node) - Optional, mainly for receiving but good practice if we ever send
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, remote_mac_health, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    ret = esp_now_add_peer(&peerInfo);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST){
        ESP_LOGW(TAG, "Failed to add health peer (might not be an error if only receiving): %s", esp_err_to_name(ret));
    }

    // Register Peer (Door Node)
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, remote_mac_door, 6);
    peerInfo.channel = 0; // Same channel
    peerInfo.encrypt = false;

    ret = esp_now_add_peer(&peerInfo);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST){
        ESP_LOGE(TAG, "Failed to add door peer: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "ESP-NOW Initialized and Peers Added");
    return ESP_OK;
}

esp_err_t app_espnow_send_command(int command) {
    control_node_data_t myData;
    myData.command = command;
    
    esp_err_t result = esp_now_send(remote_mac_control, (uint8_t *) &myData, sizeof(myData));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Sent with success: %d", command);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error sending the data");
        return ESP_FAIL;
    }
}

esp_err_t app_espnow_send_door_command(const char *command) {
    door_node_data_send_t myData;
    strncpy(myData.command, command, sizeof(myData.command) - 1);
    myData.command[sizeof(myData.command) - 1] = '\0';
    
    esp_err_t result = esp_now_send(remote_mac_door, (uint8_t *) &myData, sizeof(myData));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Sent to Door Node: %s", command);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error sending to Door Node");
        return ESP_FAIL;
    }
}
