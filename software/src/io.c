#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pin.h"
#include "io.h"

static char event_str[128];
uint8_t buf_i2c[2];

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
    gpio_set_irq_enabled_with_callback(PIN_SW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &io_button_callback);
    gpio_pull_up(PIN_SW);
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
