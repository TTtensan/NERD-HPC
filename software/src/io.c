#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pin.h"
#include "io.h"

static char event_str[128];
uint8_t buf_i2c[2];

void io_button_callback(uint gpio, uint32_t events){
    gpio_event_string(event_str, events);
    printf("GPIO %d %s\n", gpio, event_str);
}

void io_init(){
    gpio_set_irq_enabled_with_callback(PIN_SW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &io_button_callback);
    gpio_pull_up(PIN_SW);
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
