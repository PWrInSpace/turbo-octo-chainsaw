#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"

#include "esp_log.h"
#include "esp_now.h"

#include "file.h"

#define MAX_NACK_TIMEOUT 1800000
#define MIN_NACK_TIMEOUT 120000

#define MAX_PERIOD_TIMEOUT 600000
#define MIN_PERIOD_TIMEOUT 10

#define OBC_MAC_ADDRESS {0x04, 0x20, 0x04, 0x20, 0x04, 0x20}



static struct{

    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    data_to_transmit _data_to_transmit;
    on_data_rx _on_data_rx;
    uint16_t transmit_periods[ENUM_MAX];
    QueueHandle_t rx_data;
    SemaphoreHandle_t sleep_lock;
    uint8_t *tx_buffer;
    uint8_t tx_data_size;
    esp_now_send_status_t last_message_status;
    uint16_t interval_ms;
    SemaphoreHandle_t mutex;
    uint64_t last_tx_time_ms;

} sub_struct;

sub_status_t sub_init(sub_init_struct_t *init_struct, uint16_t transmit_periods[ENUM_MAX]){
    
    if(init_struct == NULL || transmit_periods == NULL){
        return SUB_NULL_ERR;
    }

    sub_struct.disable_sleep = init_struct->disable_sleep;

    if(TX_NACK_TIMEOUT_MS > MAX_NACK_TIMEOUT || TX_NACK_TIMEOUT_MS < MIN_NACK_TIMEOUT){

        return SUB_TIMES_ERR;
    }

    sub_struct.tx_nack_timeout_ms = TX_NACK_TIMEOUT_MS;

    if(init_struct->_data_to_transmit == NULL){

        return SUB_NULL_ERR;
    }

    sub_struct._data_to_transmit = init_struct->_data_to_transmit;


    for(int i = 0; i < ENUM_MAX; i++){

        if(transmit_periods[i] > MAX_PERIOD_TIMEOUT || transmit_periods[i] < MIN_PERIOD_TIMEOUT){

            return SUB_TIMES_ERR;
        }

        sub_struct.transmit_periods[i] = transmit_periods[i];
    }

    if(init_struct->_on_data_rx == NULL){

        sub_struct.rx_data = xQueueCreate(10, sizeof(uint32_t));
        if(sub_struct.rx_data == NULL){

            return SUB_QUEUE_ERR;
        }
    }
    else{

        sub_struct._on_data_rx = init_struct->_on_data_rx;
        sub_struct.rx_data = NULL;
    }

    sub_struct.sleep_lock = xSemaphoreCreateMutex();
    sub_struct.mutex = xSemaphoreCreateMutex();

    if(sub_struct.sleep_lock == NULL || sub_struct.mutex == NULL){

        return SUB_MUTEX_ERR;
    }

    sub_struct.interval_ms = sub_struct.transmit_periods[SLEEP_MS];
    
    sub_struct.last_message_status = ESP_NOW_SEND_SUCCESS;

    memcpy(sub_struct.dev_mac_address, init_struct->dev_mac_address, 6);

    if(esp_now_init() != ESP_OK){

        return SUB_ESP_NOW_ERR;
    }

    if(esp_now_register_send_cb(&send_cb) != ESP_OK){

        return SUB_ESP_NOW_ERR;
    }

    if(esp_now_register_recv_cb(&recv_cb) != ESP_OK){

        return SUB_ESP_NOW_ERR;
    }

    sub_struct.last_tx_time_ms = esp_timer_get_time();

    return SUB_OK;
}


sub_status_t sub_get_rx_data(uint8_t *data_buffer, size_t buffer_size){
    return 0;
}

static sub_status_t sleep_lock(void){
    return true;
}

static sub_status_t sleep_unlock(void){
    return true;
}

esp_now_send_status_t sub_get_last_message_status(void){

    return ESP_NOW_SEND_SUCCESS;
}

sub_status_t sub_enable_sleep(void){
    return true;
}

static sub_status_t send_cb(){

    return SUB_OK;
}

static sub_status_t recv_cb(){
    return SUB_OK;
}