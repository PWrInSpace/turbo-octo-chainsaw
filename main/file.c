#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_now.h"
#include "esp_sleep.h"

#include "file.h"

#define MAX_NACK_TIMEOUT 1800000
#define MIN_NACK_TIMEOUT 120000

#define MAX_PERIOD_TIMEOUT 600000
#define MIN_PERIOD_TIMEOUT 10

#define MCB_MAC_ADDRESS {0x04, 0x20, 0x04, 0x20, 0x04, 0x20}

#define STATE_MSG_SIZE 1

#define MAX_TX_BUFFER_SIZE 250

#define STATE_MSG_SIZE 1

#define MAX_TX_BUFFER_SIZE 256



static struct{

    uint8_t dev_mac_address[ESP_NOW_ETH_ALEN];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    data_to_transmit _data_to_transmit;
    on_data_rx _on_data_rx;
    uint16_t transmit_periods[ENUM_MAX];
    uint8_t tx_buffer[MAX_TX_BUFFER_SIZE];
    uint8_t tx_data_size;
    esp_now_send_status_t last_message_status;
    uint16_t interval_ms;
    uint64_t last_tx_time_ms;
    bool transmit_state;

    QueueHandle_t rx_queue;
    QueueHandle_t send_cb_queue;
    QueueHandle_t recv_cb_queue;
    SemaphoreHandle_t sleep_lock;
    SemaphoreHandle_t interval_mutex;
    SemaphoreHandle_t transmit_time_mutex;
    TimerHandle_t transmit_timer;
  
} sub_struct;


static void transmit_timer_cb(TimerHandle_t xTimer);

sub_status_t sub_init(sub_init_struct_t *init_struct, uint16_t transmit_periods[ENUM_MAX]){
    
    if(init_struct == NULL || transmit_periods == NULL){
        return SUB_NULL_ERR;
    }

    sub_struct.disable_sleep = init_struct->disable_sleep;

    if(TX_NACK_TIMEOUT_MS > MAX_NACK_TIMEOUT || TX_NACK_TIMEOUT_MS < MIN_NACK_TIMEOUT){

        return SUB_NACK_TMR_ERR;
    }

    sub_struct.tx_nack_timeout_ms = TX_NACK_TIMEOUT_MS;

    if(init_struct->_data_to_transmit == NULL){

        return SUB_NULL_ERR;
    }

    sub_struct._data_to_transmit = init_struct->_data_to_transmit;


    for(int i = 0; i < ENUM_MAX; i++){

        if(transmit_periods[i] > MAX_PERIOD_TIMEOUT || transmit_periods[i] < MIN_PERIOD_TIMEOUT){

            return SUB_PERIOD_ERR;
        }

        sub_struct.transmit_periods[i] = transmit_periods[i];
    }

    if(init_struct->_on_data_rx == NULL){

        sub_struct.rx_queue = xQueueCreate(1, sizeof(recv_cb_cmd_t));
        if(sub_struct.rx_queue == NULL){

            return SUB_QUEUE_ERR;
        }
    }
    else{

        sub_struct._on_data_rx = init_struct->_on_data_rx;
        sub_struct.rx_queue = NULL;
    }

    sub_struct.sleep_lock = xSemaphoreCreateMutex();
    sub_struct.interval_mutex = xSemaphoreCreateMutex();
    sub_struct.transmit_time_mutex = xSemaphoreCreateMutex();

    if(sub_struct.sleep_lock == NULL || sub_struct.interval_mutex == NULL || sub_struct.transmit_time_mutex == NULL){

        return SUB_MUTEX_ERR;
    }

    sub_struct.transmit_timer = xTimerCreate("transmit_timer", pdMS_TO_TICKS(sub_struct.transmit_periods[INIT_MS]), pdFALSE, NULL, transmit_timer_cb);

    if(sub_struct.transmit_timer == NULL){

        return SUB_TIMER_ERR;
    }

    sub_struct.transmit_state = false;

    sub_struct.interval_ms = sub_struct.transmit_periods[SLEEP_MS];
    
    sub_struct.last_message_status = ESP_NOW_SEND_SUCCESS;

    memcpy(sub_struct.dev_mac_address, init_struct->dev_mac_address, ESP_NOW_ETH_ALEN);

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

static void set_transmit_state(bool state){

    if(xSemaphoreTake(sub_struct.transmit_time_mutex, portMAX_DELAY) == pdTRUE){

        sub_struct.transmit_state = state;

        xSemaphoreGive(sub_struct.transmit_time_mutex);
    }

}

static void transmit_timer_cb(TimerHandle_t xTimer){

    //TODO: POZBYĆ SIĘ SEMAFORA Z CALLBACKA 
    //W LORZE JEST TASK NOTIFY I PEWNIE TO JEST GIT POMYSŁ
    set_transmit_state(true);
}


sub_status_t sub_get_rx_data(uint8_t *data_buffer, size_t buffer_size){
    
    recv_cb_cmd_t data;

    if(data_buffer == NULL || sub_struct.rx_queue == NULL){

        return SUB_NULL_ERR;
    }

    if(buffer_size < MAX_DATA_LENGTH){

        return SUB_RX_OVRFLW_ERR;
    }

    if(xQueueReceive(sub_struct.rx_queue, &data, 0) == pdTRUE){

        memcpy(data_buffer, data.raw, MAX_DATA_LENGTH);
    }

    return SUB_OK;
}

sub_status_t sub_sleep_lock(void){
    return xSemaphoreTake(sub_struct.sleep_lock, portMAX_DELAY) ==  pdTRUE ? SUB_OK: SUB_MUTEX_ERR;
}

sub_status_t sub_sleep_unlock(void){
    return xSemaphoreGive(sub_struct.sleep_lock) == pdTRUE ? SUB_OK: SUB_MUTEX_ERR;
}

esp_now_send_status_t sub_get_last_message_status(void){

    return sub_struct.last_message_status;
}

void sub_enable_sleep(bool enable){
    
    sub_struct.disable_sleep = !enable;

}

static void send_cb(const uint8_t *mac, esp_now_send_status_t status){

    sub_send_cb_data_t send_data;

    send_data.message_status = status;

    memcpy(send_data.mac, mac, ESP_NOW_ETH_ALEN);

    xQueueSend(sub_struct.send_cb_queue, &send_data, 0);
    
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, size_t len){

    if(info->src_addr == NULL || data == NULL || len == 0 || len > MAX_DATA_LENGTH){
        return;
    }

    sub_recv_cb_data_t recv_data;

    memcpy(recv_data.mac, info->src_addr, ESP_NOW_ETH_ALEN);
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

            if(xSemaphoreTake(sub_struct.interval_mutex, portMAX_DELAY) == pdTRUE){

                sub_struct.interval_ms = sub_struct.transmit_periods[SLEEP_MS];

                xSemaphoreGive(sub_struct.interval_mutex);
            }
        }
    }
}

static void on_recive(sub_recv_cb_data_t *recv_data){

    uint8_t mcb_mac_addr[ESP_NOW_ETH_ALEN] = MCB_MAC_ADDRESS;

    if(memcmp(recv_data->mac, mcb_mac_addr, ESP_NOW_ETH_ALEN)==0){

        if(recv_data->data_size == STATE_MSG_SIZE && recv_data->data[0] < ENUM_MAX){

            set_transmit_state(recv_data->data[0]);
        }
        else{
            recv_cb_cmd_t temp_data;

            memcpy(&temp_data, recv_data->data, recv_data->data_size);

            if(sub_struct._on_data_rx == NULL){

                if(recv_data->data_size > MAX_DATA_LENGTH){
                        
                    return;
                }

                xQueueSend(sub_struct.rx_queue, temp_data, 0);
            }
            else{
                
                sub_struct._on_data_rx(temp_data.raw, recv_data->data_size);
            }
        }

    }
}

static void send_packet(sub_send_cb_data_t send_data){

    uint8_t mcb_mac_addr[ESP_NOW_ETH_ALEN] = MCB_MAC_ADDRESS;

    if(sub_struct._data_to_transmit != NULL){

        sub_struct._data_to_transmit(&sub_struct.tx_buffer, MAX_DATA_LENGTH, &sub_struct.tx_data_size);
    }

    if(sub_struct.tx_data_size <= MAX_DATA_LENGTH && sub_struct.tx_data_size > 0 && sub_struct.tx_buffer != NULL){
        
        esp_now_send(mcb_mac_addr, sub_struct.tx_buffer, sub_struct.tx_data_size);
    }

    if(xQueueReceive(sub_struct.send_cb_queue, &send_data, 0) == pdTRUE){

        on_send(&send_data);
    }
}

static void check_for_packet(sub_recv_cb_data_t recv_data){

    if(xQueueReceive(sub_struct.recv_cb_queue, &recv_data, 0) == pdTRUE){

        on_recive(&recv_data);
    }
}

static void reset_transmit_state(void){

    if(xSemaphoreTake(sub_struct.transmit_time_mutex, portMAX_DELAY) == pdTRUE){

        sub_struct.transmit_state = false;
        
        xSemaphoreGive(sub_struct.transmit_time_mutex);
    }
}

static void sub_go_to_sleep(uint64_t interval_ms){

    if(sub_struct.disable_sleep == false){

        if(xSemaphoreTake(sub_struct.sleep_lock, portMAX_DELAY) == pdTRUE){

            esp_sleep_enable_timer_wakeup(interval_ms * 1000);
            esp_deep_sleep_start();
        }
    }

}

static bool check_transmit_period(uint64_t interval_ms){

    bool flag = false;

    for(int i = 0; i < ENUM_MAX; i++){
            
        if(sub_struct.transmit_periods[i] == interval_ms){
    
            flag = true;
            break;
        }
    }

    return flag;
}

static void run_transmit_timer(void){

    uint64_t temp_interval_ms = get_interval_ms();

    if(check_transmit_period(temp_interval_ms)){

        xTimerChangePeriod(sub_struct.transmit_timer, pdMS_TO_TICKS(temp_interval_ms), 0);
    }
}

static bool get_transmit_state(void){

    bool state = false;

    if(xSemaphoreTake(sub_struct.transmit_time_mutex, portMAX_DELAY) == pdTRUE){

        state = sub_struct.transmit_state;

        xSemaphoreGive(sub_struct.interval_mutex);
    }

    return state;
}

static bool is_sleep_interval(uint64_t interval_ms){

    return interval_ms == sub_struct.transmit_periods[SLEEP_MS];
}

static uint64_t get_interval_ms(void){

    uint64_t temp_interval_ms;

    if(xSemaphoreTake(sub_struct.interval_mutex, portMAX_DELAY) == pdTRUE){

        temp_interval_ms = sub_struct.interval_ms;

        xSemaphoreGive(sub_struct.interval_mutex);
    }

    return temp_interval_ms;
}

static void sub_task(void *pvParameters){

    sub_send_cb_data_t send_data;
    sub_recv_cb_data_t recv_data;

    while(1){
        
        bool state = get_transmit_state();

        if(state==true){

            send_packet(send_data);

            reset_transmit_state();
            run_transmit_timer();
        }
            
        check_for_packet(recv_data);

        uint64_t temp_interval_ms = get_interval_ms();

        if(is_sleep_interval(temp_interval_ms)){

            sub_go_to_sleep(temp_interval_ms);
        }

        vTaskDelay(pdMS_TO_TICK(10));

    }

}