#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// --- AIGIS SR COMMAND DEFINITIONS ---
typedef enum {
    TURN_ON_LIGHT_ONE = 1,
    TURN_OFF_LIGHT_ONE,
    TURN_ON_SOCKET,
    TURN_OFF_SOCKET,
    TURN_ON_FAN_AT_LEVEL_ONE,
    TURN_ON_FAN_AT_LEVEL_TWO,
    TURN_ON_FAN_AT_LEVEL_THREE,
    TURN_OFF_FAN
} aigis_command_t;

// Data Structure for Control Node (Light/Fan) - Sending
typedef struct {
  int command; 
} control_node_data_t;

// Data Structure for Health Node (Fall/Heart) - Receiving
typedef struct {
  bool fallDetected;
  bool healthAlarm;
  float heartRate;
  int spo2; 
} health_node_data_t;

// Data Structure for Door Node (ESP32-CAM) - Sending Command
typedef struct {
  char command[16]; 
} door_node_data_send_t;

// Data Structure for Door Node (ESP32-CAM) - Receiving Name
typedef struct {
  char name[32];
} door_node_data_recv_t;

/**
 * @brief Initialize ESP-NOW and register peer
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_espnow_init(void);

/**
 * @brief Send a command via ESP-NOW to the registered peer
 * 
 * @param command The command ID to send
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_espnow_send_command(int command);

/**
 * @brief Send a command via ESP-NOW to the Door Node
 * 
 * @param command The command string to send ("lock" or "unlock")
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_espnow_send_door_command(const char *command);
