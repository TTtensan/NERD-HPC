#include "pico/stdlib.h"
#include "pin.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "speaker.h"

float divider = 255;

uint16_t calc_top_value(uint freq){

    // PWMのカウンタはclk_sysをdividerで分周したもので動く。
    // ここは以前125000000を直書きしていたが、
    // set_sys_clock_khz()でsys_clkを変えると音程がそのままずれるので、
    // 実際のclk_sysを見て計算する
    if(freq == 0) return 0; // ゼロ除算避け

    // TOP値はwrapレジスタの都合で16bitに収まらないといけない。
    // 低すぎる周波数だとそのまま溢れて、全く違う音程になっていた
    uint32_t top_value = (uint32_t)(clock_get_hz(clk_sys) / (freq * divider));
    if(top_value > 65535) top_value = 65535;

    return (uint16_t)top_value;

}

void speaker_init(){

    uint slice_num = pwm_gpio_to_slice_num(PIN_SPEAKER);
    pwm_set_wrap(slice_num, 65535);
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    pwm_set_enabled(slice_num, true);

}

void play_sound(uint freq, uint duration_ms){

    uint slice_num = pwm_gpio_to_slice_num(PIN_SPEAKER);
    uint16_t top_value = calc_top_value(freq);

    if(top_value == 0){ // 鳴らせない周波数。無音として扱う
        pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
        if(duration_ms) sleep_ms(duration_ms);
        return;
    }

    pwm_set_wrap(slice_num, top_value);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, top_value/2);
    if(duration_ms) {
        sleep_ms(duration_ms);
        pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    }
}

void stop_sound() {
    uint slice_num = pwm_gpio_to_slice_num(PIN_SPEAKER);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
}

void play_melody() {

    uint melody[] = {262, 294, 330, 349, 392, 440, 494, 523};
    uint durations[] = {200, 200, 200, 200, 200, 200, 200, 200};
    uint i;
    for (i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
        play_sound(melody[i], durations[i]);
    }

}
