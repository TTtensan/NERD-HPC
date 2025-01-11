#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "pin.h"
#include "ir.h"

void pin_init(){

    // IO expander
    gpio_init(PIN_IOEXP_RST);
    gpio_set_dir(PIN_IOEXP_RST, GPIO_OUT);
    gpio_put(PIN_IOEXP_RST, 1);

    i2c_init(i2c0, 100000);
    gpio_set_function(PIN_IOEXP_SDA, GPIO_FUNC_I2C); // set function of SDA_PIN=GP20 I2C
    gpio_set_function(PIN_IOEXP_SCL, GPIO_FUNC_I2C); // set function of SCL_PIN=GP21 I2C
    gpio_set_pulls(PIN_IOEXP_SDA, true, false);   // enable internal pull-up of SDA_PIN=GP20
    gpio_set_pulls(PIN_IOEXP_SCL, true, false);   // enable internal pull-up of SCL_PIN=GP21

    // General IO
    uart_init(uart1, 115200);
    gpio_set_function(PIN_GEN_IO_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_GEN_IO_RX, GPIO_FUNC_UART);

    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(PIN_GEN_IO_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_GEN_IO_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_GEN_IO_SDA);
    gpio_pull_up(PIN_GEN_IO_SCL);

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
    //gpio_init(PIN_IR_TX);
    //gpio_set_dir(PIN_IR_TX, GPIO_OUT);
    //gpio_put(PIN_IR_TX, 1);

    gpio_set_function(PIN_IR_TX, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PIN_IR_TX);
    pwm_set_wrap(slice_num, 3288);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 1644);
    pwm_set_enabled(slice_num, false);

    gpio_init(PIN_IR_RX);
    gpio_set_dir(PIN_IR_RX, GPIO_IN);
    gpio_pull_up(PIN_IR_RX);

    gpio_init(PIN_SW);
    gpio_set_dir(PIN_SW, GPIO_IN);
    gpio_pull_up(PIN_SW);

    // SPEAKER
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);

}
