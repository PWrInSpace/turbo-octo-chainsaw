#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_now.h"

#include "file.h"


static struct{

    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    data_to_transmit _data_to_transmit;
    on_data_rx _on_data_rx;
    sub_transmit_periods_t *transmit_periods;
    QueueHandle_t rx_data;
    SemaphoreHandle_t sleep_lock;
    uint8_t *tx_buffer;
    uint8_t tx_data_size;
    esp_now_send_status_t last_message_status;
    uint16_t interval_ms;
    SemaphoreHandle_t mutex;

} sub_struct;

sub_status_t init(sub_init_struct_t *init_struct, sub_transmit_periods_t *transmit_periods){
    
    if(init_struct == NULL || transmit_periods == NULL){
        return SUB_NULL_ERR;
    }

    sub_struct.disable_sleep = init_struct->disable_sleep;

    if(init_struct->tx_nack_timeout_ms > 60000){

        return SUB_TIMES_ERR;
    }

    sub_struct.tx_nack_timeout_ms = init_struct->tx_nack_timeout_ms;

    if(init_struct->_data_to_transmit == NULL){

        return SUB_NULL_ERR;
    }

    sub_struct._data_to_transmit = init_struct->_data_to_transmit;


    if (transmit_periods->init_ms == 0 || transmit_periods->idle_ms == 0 || transmit_periods->armed_ms == 0 || 
        transmit_periods->filling_ms == 0 || transmit_periods->armed_to_launch_ms == 0 || transmit_periods->rdy_to_launch_ms == 0 || 
        transmit_periods->countdown_ms == 0 || transmit_periods->launch_ms == 0){

        return SUB_TIMES_ERR;
    }

    if(transmit_periods->init_ms > 60000 || transmit_periods->idle_ms > 60000 || transmit_periods->armed_ms > 60000 || 
        transmit_periods->filling_ms > 60000 || transmit_periods->armed_to_launch_ms > 60000 || transmit_periods->rdy_to_launch_ms > 60000 || 
        transmit_periods->countdown_ms > 60000 || transmit_periods->launch_ms > 60000){

        return SUB_TIMES_ERR;
    }

    sub_struct.transmit_periods = transmit_periods;

    if(init_struct->_on_data_rx == NULL){

        sub_struct.rx_data = xQueueCreate(10, sizeof(uint32_t));
    }
    else{

        sub_struct._on_data_rx = init_struct->_on_data_rx;
        sub_struct.rx_data = NULL;
    }

    sub_struct.sleep_lock = xSemaphoreCreateMutex();
    sub_struct.mutex = xSemaphoreCreateMutex();

    sub_struct.interval_ms = sub_struct.transmit_periods->sleep_ms;
    
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
}


sub_status_t get_rx_data(uint8_t *data_buffer, size_t buffer_size){
    return 0;
}

static sub_status_t sleep_lock(void){
    return true;
}

static sub_status_t sleep_unlock(void){
    return true;
}

esp_now_send_status_t get_last_message_status(void){

    return ESP_NOW_SEND_SUCCESS;
}

sub_status_t enable_sleep(void){
    return true;
}

static sub_status_t send_cb(){
    return SUB_OK;
}

static sub_status_t recv_cb(){
    return SUB_OK;
}