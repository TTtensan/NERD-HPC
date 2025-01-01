#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "pin.h"
#include "lcd.h"
#include "speaker.h"
#include "sd.hpp"
#include "com_func.h"
#include "draw.h"
#include "io.h"
#include "basic.hpp"


int main() {

    stdio_init_all();

    printf("Hello, I'm NERD BOY\n");

    srand(get_seed());
    pin_init();
    io_init();
    lcd_init();
    speaker_init();
    sd_init();

    while (true) {
        basic();
        //draw_test();
    }

}
