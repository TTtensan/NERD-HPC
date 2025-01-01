#ifndef IO_H_
#define IO_H_

static char event_str[128];

void io_button_callback(uint gpio, uint32_t events);
void io_init();
void gpio_event_string(char *buf, uint32_t events);

#endif
