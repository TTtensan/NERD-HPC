#ifndef IOEXP_H_
#define IOEXP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define IOEXP_ADDR       0x20
#define IOEXP_IODIRA     0x00
#define IOEXP_IODIRB     0x01
#define IOEXP_IPOLA      0x02
#define IOEXP_IPOLB      0x03
#define IOEXP_GPINTENA   0x04
#define IOEXP_GPINTENB   0x05
#define IOEXP_DEFVALA    0x06
#define IOEXP_DEFVALB    0x07
#define IOEXP_INTCONA    0x08
#define IOEXP_INTCONB    0x09
#define IOEXP_IOCON      0x0a
#define IOEXP_GPPUA      0x0c
#define IOEXP_GPPUB      0x0d
#define IOEXP_INTFA      0x0e
#define IOEXP_INTFB      0x0f
#define IOEXP_INTCAPA    0x10
#define IOEXP_INTCAPB    0x11
#define IOEXP_GPIOA      0x12
#define IOEXP_GPIOB      0x13
#define IOEXP_OLATA      0x14
#define IOEXP_OLATB      0x15

typedef enum {
    button_push,
    button_release
} buttondir;

char ioexp_sl2bl(char code);
char ioexp_bl2tl(char code); // 記号の切り替え
void ioexp_init();
char ioexp_getchr();
uint32_t ioexp_getchr_available();
void ioexp_write_register(uint8_t reg, uint8_t value);
void ioexp_getchrinfo(uint8_t chrinfo[2]); // キーのコードと押したか離したかの情報を渡す
short ioexp_getkey(short index); // 現在押下しているキーの配列からindex番目のキー情報を返す

extern volatile short g_en_shift, g_en_esc;

#ifdef __cplusplus
}
#endif

#endif
