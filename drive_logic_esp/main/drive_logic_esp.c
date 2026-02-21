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
#include <string.h>
#include <driver/sdspi_host.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"

// Pins
#define TWAI_TX_PIN 21
#define TWAI_RX_PIN 22
#define SD_MISO_PIN 4
#define SD_MOSI_PIN 5
#define SD_CLK_PIN 15
#define SD_CS_PIN 16

// Buffer sizes
#define CAN_DATA_SIZE 8

static const char* TAG = "drive_logic";


// TODO buffer to store data to write to CSV
// Going to take inspiration from database implementation class. Big buffer for writing.
// When buffer gets close to full, write it.



// TODO wifi based dashboard for config or seeing statistics.

// TODO start recording button that turns on LED when recording has started so I can compare that to video footage

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

esp_err_t init_sd_card() {

    esp_err_t err;

    spi_bus_config_t spi_cfg = {
            .mosi_io_num = SD_MOSI_PIN,
            .miso_io_num = SD_MISO_PIN,
            .sclk_io_num = SD_CLK_PIN,
            .quadwp_io_num = -1,
            .quadwp_io_num = -1,
            .max_transfer_sz = 400,
    };
    err = spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initialize SPI: %s", esp_err_to_name(err));
        return err;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000; // 10 MHz

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";

    err = esp_vfs_fat_sdspi_mount(mount_point,
                                  &host,
                                  &slot_config,
                                  &mount_config,
                                  &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SDSPI: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Mounted SD card successfully");

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

        uint32_t timestamp = esp_timer_get_time();
        uint32_t can_id = msg.identifier;
        uint8_t data_len = msg.data_length_code;
        uint8_t data[CAN_DATA_SIZE] = {0};
        memcpy(data, msg.data, CAN_DATA_SIZE);

    }
}


/**
 *
 */
_Noreturn void write_to_sd_task() {



    while (1) {
    }
}


void app_main(void)
{
    // Initialize everything
//    init_can();
    esp_err_t err = init_sd_card();
    if (err != ESP_OK) {
        abort();
    }

    // Create tasks
//    xTaskCreate(read_can_task, "read_can", 4096, NULL, 5, NULL);
}
