#include "pico/stdlib.h"
#include "hardware/structs/rosc.h"

unsigned int get_seed(){

    // 0で始めないと、スタック上のゴミとORされて
    // ROSCから拾ったビットがそのまま反映されない
    unsigned int seed = 0;
    for(int i=0; i<32; i++){
        seed |= rosc_hw->randombit << i;
    }
    return seed;

}
