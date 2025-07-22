#include <stdio.h>
#include <pico/i2c_slave.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pin.h"
#include "io.h"

static char event_str[128];
uint8_t buf_i2c[2];
uint8_t addr_i2c = 0x42;
uint8_t data_i2c_received = 0;

uint io_convert_pinno(uint gpio){
    switch(gpio){
        case 1:
            return PIN_GEN_IO_1;
        case 2:
            return PIN_GEN_IO_2;
        case 3:
            return PIN_GEN_IO_3;
        case 4:
            return PIN_GEN_IO_4;
    }
    return 0;
}

void io_button_callback(uint gpio, uint32_t events){
    gpio_event_string(event_str, events);
    printf("GPIO %d %s\n", gpio, event_str);
}

void io_init(){
    //gpio_set_irq_enabled_with_callback(PIN_SW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &io_button_callback);
    //gpio_pull_up(PIN_SW);
    io_i2c_master_init();
}

void io_set_dir(uint gpio, bool out){
    gpio_set_dir(io_convert_pinno(gpio), out);
}

void io_pull_up(uint gpio){
    gpio_pull_up(io_convert_pinno(gpio));
}

void io_pull_down(uint gpio){
    gpio_pull_down(io_convert_pinno(gpio));
}

void io_disable_pulls(uint gpio){
    gpio_disable_pulls(io_convert_pinno(gpio));
}

void io_put(uint gpio, bool value){
    gpio_put(io_convert_pinno(gpio), value);
}

bool io_get(uint gpio){
    return gpio_get(io_convert_pinno(gpio));
}

static const char *gpio_irq_str[] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
};

void io_uart_send(uint8_t data) {
    uart_putc_raw(UART_ID_GENIO, data);
}

uint8_t io_uart_receive() {
    if (uart_is_readable(UART_ID_GENIO)) {
        return uart_getc(UART_ID_GENIO);
    }
    return 0;
}

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
    case I2C_SLAVE_RECEIVE: // master has written some data
        data_i2c_received = i2c_read_byte_raw(i2c);
        break;
    case I2C_SLAVE_REQUEST: // master is requesting data
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        break;
    default:
        break;
    }
}

void io_i2c_master_init() {
    i2c_slave_deinit(I2C_ID_GENIO);
}

void io_i2c_slave_init() {
    i2c_slave_init(I2C_ID_GENIO, addr_i2c, &i2c_slave_handler);
}

void io_i2c_set_address(uint8_t addr) {
    addr_i2c = addr;
}

void io_i2c_send(uint8_t data) {
    uint8_t data_i2c_send = data;
    i2c_write_blocking(I2C_ID_GENIO, addr_i2c, &data_i2c_send, 1, false);
}

uint8_t io_i2c_receive() {
    return data_i2c_received;
}

void gpio_event_string(char *buf, uint32_t events) {
    for (uint i = 0; i < 4; i++) {
        uint mask = (1 << i);
        if (events & mask) {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0') {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events) {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}
