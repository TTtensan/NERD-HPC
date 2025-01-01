#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pin.h"
#include "lcd.h"
#include "speaker.h"
#include "sd.hpp"
#include "com_func.h"
#include "draw.h"
#include "io.h"
#include "ioexp.h"
#include "mcore.h"
#include "basic.hpp"

void core1_entry() {
}

int main() {

    stdio_init_all();

    printf("Hello, I'm NERD BOY\n");

    srand(get_seed());
    pin_init();
    io_init();
    ioexp_init();
    lcd_init();
    speaker_init();
    sd_init();
    mcore_init();

    multicore_launch_core1(core1_entry);

    while (true) {
        basic();
        //draw_test();
    }

}
