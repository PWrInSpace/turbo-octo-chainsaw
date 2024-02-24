#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_now.h"

typedef enum {

    SUB_OK = 0,
    SUB_NULL_ERR,
    SUB_INIT_ERR,
    SUB_PERIOD_ERR,
    SUB_ESP_NOW_ERR,
    SUB_TIMES_ERR,

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

typedef struct{

    uint16_t init_ms;
    uint16_t idle_ms;
    uint16_t armed_ms;
    uint16_t filling_ms;
    uint16_t armed_to_launch_ms;
    uint16_t rdy_to_launch_ms;
    uint16_t countdown_ms;
    uint16_t launch_ms;
    uint16_t sleep_ms;
    //...
} sub_transmit_periods_t;

esp_now_send_status_t sub_get_last_message_status(void);
bool sub_enable_sleep(void);
bool sub_init(sub_init_struct_t *init_struct, sub_transmit_periods_t transmit_periods);
size_t sub_get_rx_data(uint8_t *data_buffer, size_t buffer_size);

#endif