#ifndef FONT_H_
#define FONT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FONT_NUM 256
extern uint8_t font[FONT_NUM][5];

void font_init();
void font_setfont(uint8_t c_code, char* data);

#ifdef __cplusplus
}
#endif

#endif
