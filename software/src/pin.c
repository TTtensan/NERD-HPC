#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "pin.h"
#include "ir.h"

uint32_t ir_clock_freq;
uint32_t ir_pwm_freq;
uint32_t ir_wrap;
uint ir_slice_num;

void pin_init(){

    // IO expander
    gpio_init(PIN_IOEXP_RST);
    gpio_set_dir(PIN_IOEXP_RST, GPIO_OUT);
    gpio_put(PIN_IOEXP_RST, 1);

    gpio_init(PIN_IOEXP_INTA);
    gpio_set_dir(PIN_IOEXP_INTA, GPIO_IN);
    gpio_pull_up(PIN_IOEXP_INTA);

    i2c_init(i2c0, I2C0_BAUDRATE);
    gpio_set_function(PIN_IOEXP_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_IOEXP_SCL, GPIO_FUNC_I2C);
    gpio_set_pulls(PIN_IOEXP_SDA, true, false);
    gpio_set_pulls(PIN_IOEXP_SCL, true, false);

    // General IO
    uart_init(UART_ID_GENIO, 115200);
    gpio_set_function(PIN_GEN_IO_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_GEN_IO_RX, GPIO_FUNC_UART);

    i2c_init(I2C_ID_GENIO, 100000);
    gpio_set_function(PIN_GEN_IO_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_GEN_IO_SCL, GPIO_FUNC_I2C);
    gpio_set_pulls(PIN_GEN_IO_SDA, true, false);
    gpio_set_pulls(PIN_GEN_IO_SCL, true, false);

    gpio_init(PIN_GEN_IO_1);
    gpio_set_dir(PIN_GEN_IO_1, GPIO_OUT);
    gpio_put(PIN_GEN_IO_1, 0);

    gpio_init(PIN_GEN_IO_2);
    gpio_set_dir(PIN_GEN_IO_2, GPIO_OUT);
    gpio_put(PIN_GEN_IO_2, 0);

    gpio_init(PIN_GEN_IO_3);
    gpio_set_dir(PIN_GEN_IO_3, GPIO_OUT);
    gpio_put(PIN_GEN_IO_3, 0);

    gpio_init(PIN_GEN_IO_4);
    gpio_set_dir(PIN_GEN_IO_4, GPIO_OUT);
    gpio_put(PIN_GEN_IO_4, 0);

    // LCD
    spi_init(spi1, 8000000);

    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_SPI);

    gpio_init(PIN_LCD_RST);
    gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    gpio_put(PIN_LCD_RST, 1);

    gpio_init(PIN_LCD_CS);
    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_put(PIN_LCD_CS, 1);

    gpio_init(PIN_LCD_RS);
    gpio_set_dir(PIN_LCD_RS, GPIO_OUT);
    gpio_put(PIN_LCD_RS, 0);

    // IR
    gpio_set_function(PIN_IR_TX, GPIO_FUNC_PWM);
    ir_clock_freq = 125000000;
    ir_pwm_freq = 38000;
    ir_wrap = ir_clock_freq / ir_pwm_freq - 1; // TOP値
    ir_slice_num = pwm_gpio_to_slice_num(PIN_IR_TX);
    pwm_set_wrap(ir_slice_num, ir_wrap);
    pwm_set_chan_level(ir_slice_num, pwm_gpio_to_channel(PIN_IR_TX), ir_wrap / 2); // デューティ50%
    pwm_set_enabled(ir_slice_num, true);
    pwm_set_chan_level(ir_slice_num, pwm_gpio_to_channel(PIN_IR_TX), ir_wrap);

    gpio_init(PIN_IR_RX);
    gpio_set_dir(PIN_IR_RX, GPIO_IN);
    gpio_pull_up(PIN_IR_RX);

    gpio_init(PIN_SW);
    gpio_set_dir(PIN_SW, GPIO_IN);
    gpio_pull_up(PIN_SW);

    // SPEAKER
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);

}
