#include "pico/stdlib.h"
#include "pin.h"

void pin_init(){

    // LCD
    gpio_set_function(PIN_LCD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_SPI);

    gpio_init(PIN_LCD_CS);
    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_put(PIN_LCD_CS, 1);

    gpio_init(PIN_LCD_RS);
    gpio_set_dir(PIN_LCD_RS, GPIO_OUT);
    gpio_put(PIN_LCD_RS, 0);

    // SPEAKER
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);

}
