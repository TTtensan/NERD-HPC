#include "pico/stdlib.h"
#include "pin.h"
#include "hardware/pwm.h"
#include "speaker.h"

float divider = 255;

uint16_t calc_top_value(uint freq){

    return (125000000 / (freq * divider));

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
    pwm_set_wrap(slice_num, top_value);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, top_value/2);
    sleep_ms(duration_ms);
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
