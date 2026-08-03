#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "font.h"
#include "pin.h"
#include "lcd.h"
#include "speaker.h"
#include "sd.hpp"
#include "com_func.h"
#include "draw.h"
#include "io.h"
#include "ioexp.h"
#include "mcore.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "usb.h"
#include "basic.hpp"

void core1_entry() {
    while(true) {
        tud_task();
        usb_send_keycode_task();
        lcd_task(); // 画面転送。core0のBASICを止めないようこちらで回す
    }
}

int main() {

    // RP2040の水晶は12MHzで、sys_clkはそこからPLLで作られている。
    // 既定の125MHzはPLLの分周比で決まっているだけなので、
    // 基板に手を入れずにここで引き上げられる。
    // BASICの実行速度はほぼこの比で効く。
    // spi_init()/i2c_init()/PWMはclk_periから分周比を計算するので、
    // 必ずpin_init()より前に呼ぶこと
    set_sys_clock_khz(250000, true);

    stdio_init_all();
    tusb_init();

    printf("Hello, I'm NERD BOY\n");

    srand(get_seed());
    font_init();
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
    }

}
