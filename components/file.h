#ifdef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct init_struct_t{
    uint8_t dev_mac_address[6];
    bool disable_sleep;
    uint32_t tx_nack_timeout_ms;
    void(*data_to_transmit)(uint8_t *buffer, size_t buffer_size, size_t *tx_data_size);
    void(*on_data_rx)(uint8_t *buffer, size_t buffer_size);  
};

struct transmit_periods_t{
    uint32_t init_ms;
    uint32_t idle_ms;
    uint32_t armed_ms;
    uint32_t filling_ms;
    uint32_t armed_to_launch_ms;
    uint32_t rdy_to_launch_ms;
    uint32_t countdown_ms;
    uint32_t launch_ms;
    //...
};

#endif