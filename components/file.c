#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_now.h"

#include "file.h"


struct lib_struct{

    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    void (*data_to_transmit)(uint8_t *buffer, size_t buffer_size, size_t *tx_data_size);
    void (*on_data_recieved)(uint8_t *buffer, size_t buffer_size);
    transmit_periods_t transmit_periods;
    QueueHandle_t rx_data;
    SemaphoreHandle_t sleep_lock;
    uint8_t tx_buffer[];
    uint8_t tx_data_size
    esp_now_status_t last_message_status;
};

bool init(init_struct_t *init_struct, transmit_periods_t transmit_periods){
    return true;
}

size_t get_rx_data(uint8_t *data_buffer, size_t buffer_size){
    return 0;
}

bool sleep_lock(void){
    return true;
}

bool sleep_unlock(void){
    return true;
}

esp_now_status_t get_last_message_status(void){
    return ESP_OK;
}

bool sleep_enabled(void){
    return true;
}