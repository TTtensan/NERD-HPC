#ifndef SPEAKER_H_
#define SPEAKER_H_

#include "pico/stdlib.h"

uint16_t calc_top_value(uint freq);
void speaker_init();
void play_sound(uint freq, uint duration_ms);
void play_melody();

#endif
