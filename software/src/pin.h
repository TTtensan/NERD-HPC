#ifndef PIN_H_
#define PIN_H_

#include "hardware/i2c.h"

#define PIN_IOEXP_SDA 0
#define PIN_IOEXP_SCL 1
#define PIN_IOEXP_INTA 2
#define PIN_IOEXP_RST 3

#define PIN_GEN_IO_TX 4
#define PIN_GEN_IO_RX 5
#define PIN_GEN_IO_SDA 6
#define PIN_GEN_IO_SCL 7

#define PIN_LCD_RST 8
#define PIN_LCD_CS 9
#define PIN_LCD_SCL 10
#define PIN_LCD_SDA 11
#define PIN_LCD_RS 12

#define PIN_IR_TX 14
#define PIN_IR_RX 15

#define PIN_SD_DAT0 16
#define PIN_SD_DAT3 17
#define PIN_SD_CLK 18
#define PIN_SD_CMD 19

#define PIN_SPEAKER 21
#define PIN_SW 22

#define PIN_GEN_IO_1 25
#define PIN_GEN_IO_2 26
#define PIN_GEN_IO_3 27
#define PIN_GEN_IO_4 28

#define UART_ID_GENIO uart1
#define I2C_ID_GENIO i2c1

void pin_init();

#endif
