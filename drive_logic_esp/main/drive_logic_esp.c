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
#define BUILT_IN_LED 2

// Buffer sizes
#define CAN_DATA_SIZE 8
#define CSV_LINE_SIZE 40 // How many chars per line in the CSV
#define STREAM_BUFFER_MAX_MSGS 1500 // How many msgs max in the stream buffer
#define STREAM_BUFFER_DUMP_PERCENT 0.7 // At what percent fill to trigger a CSV write

static const char* TAG = "drive_logic";


typedef struct {
    uint32_t timestamp;
    uint32_t id;
    uint8_t data_length_code;
    uint8_t data[CAN_DATA_SIZE];
} can_msg_t;


// TODO buffer to store data to write to CSV
// Going to take inspiration from database implementation class. Big buffer for writing.
// When buffer gets close to full, write it.
QueueHandle_t can_data_queue = NULL;
StreamBufferHandle_t csv_stream_queue = NULL;


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

/**
 * Initialize and mount SD card for data logging
 * @return error if an error was encountered, otherwise ESP_OK
 */
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
 * Task to continuously read data from the CAN bus and package it into CAN
 * msg structs for further reading.
 */
_Noreturn void read_can_task() {

    twai_message_t twai_msg;

    while (1) {
        // Receive data from TWAI
        esp_err_t err = twai_receive(&twai_msg, portMAX_DELAY);
        if (err != ESP_OK) {
            if (err == ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Timed out waiting to receive data over CAN");
            } else {
                ESP_LOGE(TAG, "Unable to receive data from TWAI (CAN): %s", esp_err_to_name(err));
            }
            continue;
        }

        // Create CAN msg struct from this message
        can_msg_t can_msg;
        can_msg.timestamp = esp_timer_get_time();
        can_msg.id = twai_msg.identifier;
        can_msg.data_length_code = twai_msg.data_length_code;
        memcpy(can_msg.data, twai_msg.data, CAN_DATA_SIZE);

        // Put CAN message into queue for future processing
        if (xQueueSend(can_data_queue, &can_msg, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Dropped CAN frame, as CAN data queue is full!");
        }
    }
}

/**
 * Convert CAN msg structs into CSVs to put into a stream.
 */
_Noreturn void convert_can_to_csv_task() {

    can_msg_t can_msg;

    while (1) {
        // Get next CAN message from the queue
        if (xQueueReceive(can_data_queue, &can_msg, portMAX_DELAY)) {

            // Convert CAN message to string for CSV
            char buffer[CSV_LINE_SIZE];
            sprintf(buffer, "%lu,0x%lx,0x%x,0x%s\n", can_msg.timestamp, can_msg.id, can_msg.data_length_code, can_msg.data);

            if (xStreamBufferSend(csv_stream_queue, buffer, strlen(buffer) + 1, pdMS_TO_TICKS(2000)) == 0) {
                ESP_LOGW(TAG, "Timed out sending data to stream buffer for SD card writing");
            }
        }
    }
}


/**
 * Write buffer stream of CSV data to a CSV whenever the buffer gets full.
 */
_Noreturn void write_to_sd_task() {

    uint8_t buffer[CSV_LINE_SIZE * STREAM_BUFFER_MAX_MSGS];


    while (1) {
        // Wait until the queue hits the trigger level to write to SD card.
        size_t received = xStreamBufferReceive(
                csv_stream_queue,
                buffer,
                sizeof(buffer),
                portMAX_DELAY
                );
        if (received > 0) {
            fwrite(buffer, 1, received, log_file);
            fflush(log_file);
        }
    }
}


void app_main(void)
{
    // Initialize queues
    can_data_queue = xQueueCreate(1000, sizeof(can_msg_t));
    csv_stream_queue = xStreamBufferCreate(CSV_LINE_SIZE * STREAM_BUFFER_MAX_MSGS, STREAM_BUFFER_DUMP_PERCENT * CSV_LINE_SIZE * STREAM_BUFFER_MAX_MSGS);

    // Initialize GPIO
    init_can();
    esp_err_t err = init_sd_card();
    if (err != ESP_OK) {
        abort();
    }

    // TODO init SD card with new file for this run

    // Create tasks
    xTaskCreate(read_can_task, "read_can", 4096, NULL, 5, NULL);
}
