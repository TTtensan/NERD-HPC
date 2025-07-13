#include <inttypes.h>
#include "stdio.h"
#include "pico/stdlib.h"
#include "pin.h"
#include "hardware/pwm.h"
#include "ir.h"

uint64_t arry_ir_interval[1280] = {0};

void ir_put(bool value) {
  if(value) {
    pwm_set_chan_level(ir_slice_num, pwm_gpio_to_channel(PIN_IR_TX), ir_wrap / 2); // デューティ50%
  } else {
    pwm_set_chan_level(ir_slice_num, pwm_gpio_to_channel(PIN_IR_TX), ir_wrap); // デューティ50%
  }
}

bool ir_get() {
  return !gpio_get(PIN_IR_RX);
}

void ir_copy() {
    for(int i=0; i<1280; i++) arry_ir_interval[i] = 0;
    //to_us_since_boot(get_absolute_time());
    //printf( "%" PRIu64 "\n", to_us_since_boot(get_absolute_time()));
    int index = 0;
    while(true) {
        if(!gpio_get(PIN_IR_RX)) {
            uint64_t count_start = to_us_since_boot(get_absolute_time());
            while(!gpio_get(PIN_IR_RX));
            arry_ir_interval[index] = to_us_since_boot(get_absolute_time()) - count_start;
            index++;
            if(index == 1270) {
                printf("index over flow\n");
            }
            count_start = to_us_since_boot(get_absolute_time());
            while(gpio_get(PIN_IR_RX)) {
                if(!gpio_get(PIN_SW))
                {
                    printf("copied\n");
                    sleep_ms(1000);
                    return;
                }
            }
            arry_ir_interval[index] = to_us_since_boot(get_absolute_time()) - count_start;
            index++;
        }

    }
}

void ir_send_copied() {
    while(gpio_get(PIN_SW));
    printf("send\n");
    sleep_ms(500);
    bool on_off = true;
    for(int i=0; i<1280; i++) {
        pwm_set_enabled(pwm_gpio_to_slice_num(PIN_IR_TX), on_off);
        on_off = !on_off;
        if(arry_ir_interval[i] == 0) return;
        sleep_us(arry_ir_interval[i]);
    }
}
