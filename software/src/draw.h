#ifndef DRAW_H_
#define DRAW_H_

#include "lcd.h"

void draw_place_dot_order(uint8_t row_start, uint8_t row_end, color cl);
void draw_place_dot_random(color cl);
void draw_place_c_order(color cl);
void draw_test();

#endif
