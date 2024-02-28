#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "esp_now.h"

#define TX_NACK_TIMEOUT_MS 600000
typedef enum {

    SUB_OK = 0,
    SUB_NULL_ERR,
    SUB_PERIOD_ERR,
    SUB_ESP_NOW_ERR,
    SUB_TIMES_ERR,
    SUB_QUEUE_ERR,
    SUB_MUTEX_ERR

} sub_status_t;

typedef void (*data_to_transmit)(uint8_t *buffer, size_t buffer_size, size_t *tx_data_size);
typedef void (*on_data_rx)(uint8_t *buffer, size_t buffer_size);

typedef struct {

    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    data_to_transmit _data_to_transmit;
    on_data_rx _on_data_rx;

} sub_init_struct_t;

typedef struct {

    esp_now_send_status_t message_status;
    uint8_t mac[ESP_NOW_ETH_ALEN];
} sub_send_cb_data_t;

typedef struct {
    
    uint8_t mac[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    size_t data_size;
} sub_recv_cb_data_t;

typedef enum{

    INIT_MS = 0,
    IDLE_MS,
    ARMED_MS,
    FILLING_MS,
    ARMED_TO_LAUNCH_MS,
    RDY_TO_LAUNCH_MS,
    COUNTDOWN_MS,
    LAUNCH_MS,
    //...
    SLEEP_MS,
    ENUM_MAX

} sub_transmit_period_t;


esp_now_send_status_t sub_get_last_message_status(void);
sub_status_t sub_enable_sleep(void);
sub_status_t sub_init(sub_init_struct_t *init_struct, uint16_t transmit_periods[ENUM_MAX]);
sub_status_t sub_get_rx_data(uint8_t *data_buffer, size_t buffer_size);

#endif