#ifndef PIN_H_
#define PIN_H_

#define PIN_SW 22
#define PIN_SPEAKER 25

#define PIN_RP_TX 0
#define PIN_RP_RX 1

#define PIN_LCD_RS 15
#define PIN_LCD_MISO 15
#define PIN_LCD_CS 17
#define PIN_LCD_SCL 18
#define PIN_LCD_SDA 19

#define PIN_SD_CLK 10
#define PIN_SD_CMD 11
#define PIN_SD_DAT0 12
#define PIN_SD_DAT3 13

#define PIN_GEN_IO_1 20
#define PIN_GEN_IO_2 21
#define PIN_GEN_IO_ADC1 26
#define PIN_GEN_IO_ADC2 27

#define PIN_GEN_IO_SDA 4
#define PIN_GEN_IO_SCL 5
#define PIN_GEN_IO_TX 8
#define PIN_GEN_IO_RX 9

void pin_init();

#endif
