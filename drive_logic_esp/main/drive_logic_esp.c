///////////////////////////////////////////////////////////////////////////////
//
// File: drive_logic_esp.c
//
// Author: Isaac Ingram
//
// Licensed under GNU GPL v3.0, see LICENSE for more info.
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// Pins
#define TWAI_TX_PIN 21
#define TWAI_RX_PIN 22

static const char* TAG = "drive_logic";


/**
 * Initialize the CAN bus using TWAI
 * @return error if an error was encountered, otherwise ESP_OK
 */
esp_err_t init_can() {
    // Initialize configurations
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TWAI_TX_PIN, TWAI_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        return err;
    }

    // Start TWAI driver
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver");
        return err;
    }

    return ESP_OK;
}

/**
 * Task to continuously read and process messages from the CAN bus.
 */
_Noreturn void read_can_task() {

    twai_message_t msg;

    while (1) {
        // Receive data from TWAI
        esp_err_t err = twai_receive(&msg, portMAX_DELAY);
        if (err != ESP_OK) {
            if (err == ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Timed out waiting to receive data over CAN");
            } else {
                ESP_LOGE(TAG, "Unable to receive data from TWAI (CAN): %s", esp_err_to_name(err));
            }
            continue;
        }

        // TODO process CAN messages

    }
}

void app_main(void)
{
    // Initialize everything
    init_can();

    // Create tasks
    xTaskCreate(read_can_task, "read_can", 4096, NULL, 5, NULL);
}
