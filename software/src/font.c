#include <stdio.h>
#include "font.h"

const uint8_t font[FONT_NUM][5] =
{
    { // " " 0x20
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    },

    { // ! 0x21
        0b00000000,
        0b00000000,
        0b01001111,
        0b00000000,
        0b00000000
    },

    { // " 0x22
        0b00000000,
        0b00000111,
        0b00000000,
        0b00000111,
        0b00000000
    },

    { // # 0x23
        0b00010100,
        0b01111111,
        0b00010100,
        0b01111111,
        0b00010100
    },

    { // $ 0x24
        0b00100100,
        0b00101010,
        0b01111111,
        0b00101010,
        0b00010010
    },

    { // % 0x25
        0b00100011,
        0b00010011,
        0b00001000,
        0b01100100,
        0b01100010
    },

    { // & 0x26
        0b00110110,
        0b01001001,
        0b01010101,
        0b00100010,
        0b01010101
    },

    { // ' 0x27
        0b00000000,
        0b00000101,
        0b00000011,
        0b00000000,
        0b00000000
    },

    { // ( 0x28
        0b00000000,
        0b00011100,
        0b00100010,
        0b01000001,
        0b00000000
    },

    { // ) 0x29
        0b00000000,
        0b01000001,
        0b00100010,
        0b00011100,
        0b00000000
    },

    { // * 0x2a
        0b00010100,
        0b00001000,
        0b00111110,
        0b00001000,
        0b00010100
    },

    { // + 0x2b
        0b00001000,
        0b00001000,
        0b00111110,
        0b00001000,
        0b00001000
    },

    { // , 0x2c
        0b00000000,
        0b01010000,
        0b00110000,
        0b00000000,
        0b00000000
    },

    { // - 0x2d
        0b00001000,
        0b00001000,
        0b00001000,
        0b00001000,
        0b00001000
    },

    { // . 0x2e
        0b00000000,
        0b01100000,
        0b01100000,
        0b00000000,
        0b00000000
    },

    { // / 0x2f
        0b00100000,
        0b00010000,
        0b00001000,
        0b00000100,
        0b00000010
    },

    { // 0 0x30
        0b00111110,
        0b01010001,
        0b01001001,
        0b01000101,
        0b00111110
    },

    { // 1 0x31
        0b00000000,
        0b01000010,
        0b01111111,
        0b01000000,
        0b00000000
    },

    { // 2 0x32
        0b01000010,
        0b01100001,
        0b01010001,
        0b01001001,
        0b01000110
    },

    { // 3 0x33
        0b00100001,
        0b01000001,
        0b01000101,
        0b01001011,
        0b00110001
    },

    { // 4 0x34
        0b00011000,
        0b00010100,
        0b00010010,
        0b01111111,
        0b00010000
    },

    { // 5 0x35
        0b00100111,
        0b01000101,
        0b01000101,
        0b01000101,
        0b00111001
    },

    { // 6 0x36
        0b00111100,
        0b01001010,
        0b01001001,
        0b01001001,
        0b00110000
    },

    { // 7 0x37
        0b00000001,
        0b01110001,
        0b00001001,
        0b00000101,
        0b00000011
    },

    { // 8 0x38
        0b00110110,
        0b01001001,
        0b01001001,
        0b01001001,
        0b00110110
    },

    { // 9 0x39
        0b00000110,
        0b01001001,
        0b01001001,
        0b00101001,
        0b00011110
    },

    { // : 0x3a
        0b00000000,
        0b00110110,
        0b00110110,
        0b00000000,
        0b00000000
    },

    { // ; 0x3b
        0b00000000,
        0b01010110,
        0b00110110,
        0b00000000,
        0b00000000
    },

    { // < 0x3c
        0b00001000,
        0b00010100,
        0b00100010,
        0b01000001,
        0b00000000
    },

    { // = 0x3d
        0b00010100,
        0b00010100,
        0b00010100,
        0b00010100,
        0b00010100
    },

    { // > 0x3e
        0b00000000,
        0b01000001,
        0b00100010,
        0b00010100,
        0b00001000
    },

    { // ? 0x3f
        0b00000010,
        0b00000001,
        0b01010001,
        0b00001001,
        0b00000110
    },

    { // @ 0x40
        0b00110010,
        0b01001001,
        0b01111001,
        0b01000001,
        0b00111110
    },

    { // A 0x41
        0b01111110,
        0b00010001,
        0b00010001,
        0b00010001,
        0b01111110
    },

    { // B 0x42
        0b01111111,
        0b01001001,
        0b01001001,
        0b01001001,
        0b00110110
    },

    { // C 0x43
        0b00111110,
        0b01000001,
        0b01000001,
        0b01000001,
        0b00100010
    },

    { // D 0x44
        0b01111111,
        0b01000001,
        0b01000001,
        0b00100010,
        0b00011100
    },

    { // E 0x45
        0b01111111,
        0b01001001,
        0b01001001,
        0b01001001,
        0b01000001
    },

    { // F 0x46
        0b01111111,
        0b00001001,
        0b00001001,
        0b00001001,
        0b00000001
    },

    { // G 0x47
        0b00111110,
        0b01000001,
        0b01001001,
        0b01001001,
        0b01111010
    },

    { // H 0x48
        0b01111111,
        0b00001000,
        0b00001000,
        0b00001000,
        0b01111111
    },

    { // I 0x49
        0b00000000,
        0b01000001,
        0b01111111,
        0b01000001,
        0b00000000
    },

    { // J 0x4a
        0b00100000,
        0b01000000,
        0b01000001,
        0b00111111,
        0b00000001
    },

    { // K 0x4b
        0b01111111,
        0b00001000,
        0b00010100,
        0b00100010,
        0b01000001
    },

    { // L 0x4c
        0b01111111,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000
    },

    { // M 0x4d
        0b01111111,
        0b00000010,
        0b00001100,
        0b00000010,
        0b01111111
    },

    { // N 0x4e
        0b01111111,
        0b00000100,
        0b00001000,
        0b00010000,
        0b01111111
    },

    { // O 0x4f
        0b00111110,
        0b01000001,
        0b01000001,
        0b01000001,
        0b00111110
    },

    { // P 0x50
        0b01111111,
        0b00001001,
        0b00001001,
        0b00001001,
        0b00000110
    },

    { // Q 0x51
        0b00111110,
        0b01000001,
        0b01010001,
        0b00100001,
        0b01011110
    },

    { // R 0x52
        0b01111111,
        0b00001001,
        0b00011001,
        0b00101001,
        0b01000110
    },

    { // S 0x53
        0b01000110,
        0b01001001,
        0b01001001,
        0b01001001,
        0b00110001
    },

    { // T 0x54
        0b00000001,
        0b00000001,
        0b01111111,
        0b00000001,
        0b00000001
    },

    { // U 0x55
        0b00111111,
        0b01000000,
        0b01000000,
        0b01000000,
        0b00111111
    },

    { // V 0x56
        0b00011111,
        0b00100000,
        0b01000000,
        0b00100000,
        0b00011111
    },

    { // W 0x57
        0b00111111,
        0b01000000,
        0b00111000,
        0b01000000,
        0b00111111
    },

    { // X 0x58
        0b01100011,
        0b00010100,
        0b00001000,
        0b00010100,
        0b01100011
    },

    { // Y 0x59
        0b00000111,
        0b00001000,
        0b01110000,
        0b00001000,
        0b00000111
    },

    { // Z 0x5a
        0b01100001,
        0b01010001,
        0b01001001,
        0b01000101,
        0b01000011
    },

    { // [ 0x5b
        0b00000000,
        0b01111111,
        0b01000001,
        0b01000001,
        0b00000000
    },

    { // "\" 0x5c
        0b00000010,
        0b00000100,
        0b00001000,
        0b00010000,
        0b00100000
    },

    { // ] 0x5d
        0b00000000,
        0b01000001,
        0b01000001,
        0b01111111,
        0b00000000
    },

    { // ^ 0x5e
        0b00000100,
        0b00000010,
        0b00000001,
        0b00000010,
        0b00000100
    },

    { // _ 0x5f
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000
    },

    { // ` 0x60
        0b00000000,
        0b00000001,
        0b00000010,
        0b00000100,
        0b00000000
    },

    { // a 0x61
        0b00100000,
        0b01010100,
        0b01010100,
        0b01010100,
        0b01111000
    },

    { // b 0x62
        0b01111111,
        0b01001000,
        0b01000100,
        0b01000100,
        0b00111000
    },

    { // c 0x63
        0b00111000,
        0b01000100,
        0b01000100,
        0b01000100,
        0b00100000
    },

    { // d 0x64
        0b00111000,
        0b01000100,
        0b01000100,
        0b01001000,
        0b01111111
    },

    { // e 0x65
        0b00111000,
        0b01010100,
        0b01010100,
        0b01010100,
        0b00011000
    },

    { // f 0x66
        0b00001000,
        0b01111110,
        0b00001001,
        0b00000001,
        0b00000010
    },

    { // g 0x67
        0b00001100,
        0b01010010,
        0b01010010,
        0b01010010,
        0b00111110
    },

    { // h 0x68
        0b01111111,
        0b00001000,
        0b00000100,
        0b00000100,
        0b01111000
    },

    { // i 0x69
        0b00000000,
        0b01000100,
        0b01111101,
        0b01000000,
        0b00000000
    },

    { // j 0x6a
        0b00100000,
        0b01000000,
        0b01000100,
        0b00111101,
        0b00000000
    },

    { // k 0x6b
        0b01111111,
        0b00010000,
        0b00101000,
        0b01000100,
        0b00000000
    },

    { // l 0x6c
        0b00000000,
        0b01000001,
        0b01111111,
        0b01000000,
        0b00000000
    },

    { // m 0x6d
        0b01111100,
        0b00000100,
        0b00011000,
        0b00000100,
        0b01111000
    },

    { // n 0x6e
        0b01111100,
        0b00001000,
        0b00000100,
        0b00000100,
        0b01111000
    },

    { // o 0x6f
        0b00111000,
        0b01000100,
        0b01000100,
        0b01000100,
        0b00111000
    },

    { // p 0x70
        0b01111100,
        0b00010100,
        0b00010100,
        0b00010100,
        0b00001000
    },

    { // q 0x71
        0b00001000,
        0b00010100,
        0b00010100,
        0b00011000,
        0b01111100
    },

    { // r 0x72
        0b01111100,
        0b00001000,
        0b00000100,
        0b00000100,
        0b00001000
    },

    { // s 0x73
        0b01001000,
        0b01010100,
        0b01010100,
        0b01010100,
        0b00100000
    },

    { // t 0x74
        0b00000100,
        0b00111111,
        0b01000100,
        0b01000000,
        0b00100000
    },

    { // u 0x75
        0b00111100,
        0b01000000,
        0b01000000,
        0b00100000,
        0b01111100
    },

    { // v 0x76
        0b00011100,
        0b00100000,
        0b01000000,
        0b00100000,
        0b00011100
    },

    { // w 0x77
        0b00111100,
        0b01000000,
        0b00110000,
        0b01000000,
        0b00111100
    },

    { // x 0x78
        0b01000100,
        0b00101000,
        0b00010000,
        0b00101000,
        0b01000100
    },

    { // y 0x79
        0b00001100,
        0b01010000,
        0b01010000,
        0b01010000,
        0b00111100
    },

    { // z 0x7a
        0b01000100,
        0b01100100,
        0b01010100,
        0b01001100,
        0b01000100
    },

    { // { 0x7b
        0b00000000,
        0b00001000,
        0b00110110,
        0b01000001,
        0b00000000
    },

    { // | 0x7c
        0b00000000,
        0b00000000,
        0b01111111,
        0b00000000,
        0b00000000
    },

    { // } 0x7d
        0b00000000,
        0b01000001,
        0b00110110,
        0b00001000,
        0b00000000
    },

    { // ~ 0x7e
        0b00001000,
        0b00000100,
        0b00001000,
        0b00010000,
        0b00001000
    },

    { // DEL 0x7f
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    }

};
