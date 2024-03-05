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

#define STATE_MSG_SIZE 1

#define MAX_TX_BUFFER_SIZE 250



static struct{

    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    data_to_transmit _data_to_transmit;
    on_data_rx _on_data_rx;
    uint16_t transmit_periods[ENUM_MAX];
    QueueHandle_t rx_queue;
    QueueHandle_t send_cb_queue;
    QueueHandle_t recv_cb_queue;
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

        sub_struct.rx_queue = xQueueCreate(10, sizeof(uint32_t));
        if(sub_struct.rx_queue == NULL){

            return SUB_QUEUE_ERR;
        }
    }
    else{

        sub_struct._on_data_rx = init_struct->_on_data_rx;
        sub_struct.rx_queue = NULL;
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

    sub_struct.send_cb_queue = xQueueCreate(1, sizeof(sub_send_cb_data_t));

    if(sub_struct.send_cb_queue == NULL){

        return SUB_QUEUE_ERR;
    }

    sub_struct.recv_cb_queue = xQueueCreate(1, sizeof(sub_recv_cb_data_t));

    if(sub_struct.recv_cb_queue == NULL){

        return SUB_QUEUE_ERR;
    }

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

    return sub_struct.last_message_status;
}

sub_status_t sub_enable_sleep(void){
    return true;
}

static void send_cb(const uint8_t *mac, esp_now_send_status_t status){

    sub_send_cb_data_t send_data;

    send_data.message_status = status;

    memcpy(send_data.mac, mac, ESP_NOW_ETH_ALEN);

    xQueueSend(sub_struct.send_cb_queue, &send_data, 0);
    
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, size_t len){

    if(info->src_addr == NULL || data == NULL || len == 0){
        return;
    }

    sub_recv_cb_data_t recv_data;

    memcpy(recv_data.mac, info->src_addr, ESP_NOW_ETH_ALEN);

    if(len > MAX_DATA_LENGTH){
        return;
    }

    memcpy(recv_data.data, data, len);
    recv_data.data_size = len;

    xQueueSend(sub_struct.recv_cb_queue, &recv_data, 0);

}

static void on_send(sub_send_cb_data_t *send_data){


    uint64_t current_time_ms = esp_timer_get_time()/1000;

    sub_struct.last_message_status = send_data->message_status;

    if(send_data->message_status == ESP_NOW_SEND_SUCCESS){

        sub_struct.last_tx_time_ms = current_time_ms;

    }
    else{

        if(current_time_ms - sub_struct.last_tx_time_ms > sub_struct.tx_nack_timeout_ms){

            if(xSemaphoreTake(sub_struct.mutex, portMAX_DELAY) == pdTRUE){

                sub_struct.interval_ms = sub_struct.transmit_periods[SLEEP_MS];

                xSemaphoreGive(sub_struct.mutex);

            }
        }
    }
}

static void on_recive(sub_recv_cb_data_t *recv_data){

    uint8_t obc_mac_addr[ESP_NOW_ETH_ALEN] = OBC_MAC_ADDRESS;

    if(memcmp(recv_data->mac, obc_mac_addr, ESP_NOW_ETH_ALEN)==0){

        if(recv_data->data_size == STATE_MSG_SIZE && recv_data->data[0] < ENUM_MAX){

            if(xSemaphoreTake(sub_struct.mutex, portMAX_DELAY) == pdTRUE){

                sub_struct.interval_ms = sub_struct.transmit_periods[recv_data->data[0]];

                xSemaphoreGive(sub_struct.mutex);
            }
        }
    }
    else{

        if(sub_struct._on_data_rx == NULL){

            xQueueSend(sub_struct.rx_queue, recv_data->data, 0);
        }
        else{

            sub_struct._on_data_rx(recv_data->data, recv_data->data_size);
        }
    }
    
}