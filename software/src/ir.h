#ifndef IR_H_
#define IR_H_

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t ir_clock_freq;
extern uint32_t ir_pwm_freq;
extern uint32_t ir_wrap; // TOP値
extern uint ir_slice_num;

void ir_put(bool value);
bool ir_get();
void ir_copy();
void ir_send_copied();

#ifdef __cplusplus
}
#endif

#endif
