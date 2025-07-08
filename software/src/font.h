#ifndef FONT_H_
#define FONT_H_

#include <stdint.h>

#define FONT_NUM 256
extern uint8_t font[FONT_NUM][5];

void font_init();

#endif
