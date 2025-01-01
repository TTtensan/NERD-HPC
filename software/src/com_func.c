#include "pico/stdlib.h"
#include "hardware/structs/rosc.h"

unsigned int get_seed(){

    unsigned int seed;
    for(int i=0; i<32; i++){
        seed |= rosc_hw->randombit << i;
    }
    return seed;

}
