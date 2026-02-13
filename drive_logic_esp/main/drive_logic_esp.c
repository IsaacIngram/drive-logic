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


void app_main(void)
{
    init_can();
}
