#ifndef MCORE_H_
#define MCORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pico/multicore.h"

extern semaphore_t sem;

void mcore_init();

#ifdef __cplusplus
}
#endif

#endif
