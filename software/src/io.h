#ifndef IO_H_
#define IO_H_

#ifdef __cplusplus
extern "C" {
#endif

static char event_str[128];

void io_button_callback(uint gpio, uint32_t events);
void io_init();
void io_set_dir(uint gpio, bool out);
void io_pull_up(uint gpio);
void io_pull_down(uint gpio);
void io_disable_pulls(uint gpio);
void io_put(uint gpio, bool value);
bool io_get(uint gpio);
void io_uart_send(uint8_t data);
uint8_t io_uart_get();
void gpio_event_string(char *buf, uint32_t events);

#ifdef __cplusplus
}
#endif

#endif
