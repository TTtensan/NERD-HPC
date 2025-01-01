#include "pico/multicore.h"
#include "mcore.h"

semaphore_t sem;

void mcore_init() {
    sem_init(&sem, 1, 1);
}
