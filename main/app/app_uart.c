/**
 * @file app_uart.c
 * @brief UART communication with Arduino Nano
 */

#include "app_uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "app_uart";

#define UART_NUM                UART_NUM_1
#define UART_TX_PIN             42
#define UART_RX_PIN             13
#define UART_BAUD_RATE          115200
#define UART_BUF_SIZE           1024

esp_err_t app_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "Initializing UART connection to Nano on TX=%d, RX=%d", UART_TX_PIN, UART_RX_PIN);
    
    // Install UART driver
    esp_err_t err = uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return err;
    }

    // Configure UART parameters
    err = uart_param_config(UART_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(err));
        return err;
    }

    // Set UART pins
    err = uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "UART initialized successfully");
    return ESP_OK;
}

esp_err_t app_uart_send_cmd(char cmd)
{
    char data[2] = {cmd, '\0'}; // Send as string or single char
    int txBytes = uart_write_bytes(UART_NUM, data, 1);
    
    if (txBytes > 0) {
        ESP_LOGI(TAG, "Sent command to Nano: '%c'", cmd);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to send command: '%c'", cmd);
        return ESP_FAIL;
    }
}
