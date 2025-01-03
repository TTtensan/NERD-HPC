#ifndef SPEAKER_H_
#define SPEAKER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pico/stdlib.h"

uint16_t calc_top_value(uint freq);
void speaker_init();
void play_sound(uint freq, uint duration_ms);
void stop_sound();
void play_melody();

#ifdef __cplusplus
}
#endif

#endif
