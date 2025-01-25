#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pin.h"
#include "ioexp.h"

static struct repeating_timer ioexp_timer;

// 前回スキャン時のキーの押下情報 1ビットで記録 0=押下 1=リリース
volatile uint8_t prev_keyinfo[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
volatile bool flg_getchr_available = false;
volatile uint8_t current_chr = 0x00;

volatile bool status_shift = false;
volatile bool status_caps = true;
volatile bool status_2ndfn = false;

volatile short g_en_shift = 1;
volatile short g_en_esc = 1;

uint8_t table_key2code[2][8][8] = { // Shiftを押した状態も考慮
{ // 何も押していない時
    {0x0e, CODE_CTRL, CODE_ALT, 0x20, CODE_UP, CODE_DOWN, CODE_LEFT, CODE_RIGHT}, // L-Shift, L-Ctrl, L-Alt, Space, ↑, ↓, ←, →
    {0x0f, 0x2d, 0x5b, 0x61, 0x73, 0x74, 0x72, 0x34}, // R-Shift, -, [, a, s, t, r, 4
    {CODE_NERD, 0x3d, 0x27, 0x7a, 0x64, 0x79, 0x65, 0x35}, // NERD, =, ', z, d, y, e, 5
    {0x0d, 0x5d, 0x3b, 0x78, 0x66, 0x75, 0x77, 0x36}, // Enter, ], ;, x, f, u, w, 6
    {0x09, 0x5c, 0x2f, 0x63, 0x67, 0x69, 0x71, 0x37}, // Tab, \, /, c, g, i, q, 7
    {0x1b, 0x60, 0x2e, 0x76, 0x68, 0x6f, 0x31, 0x38}, // Esc, `, ., v, h, o, 1, 8
    {CODE_CAPS, 0x08, 0x2c, 0x62, 0x6a, 0x70, 0x32, 0x39}, // Caps, BS, ,, b, j, p, 2, 9
    {CODE_2NDFN, CODE_INS, 0x6d, 0x6e, 0x6b, 0x6c, 0x33, 0x30}  // 2ndFn, Ins, m, n, k, l, 3, 0
},
{ // Shift押した状態
    {0x0e, CODE_CTRL, CODE_ALT, 0x20, CODE_UP, CODE_DOWN, CODE_LEFT, CODE_RIGHT}, // L-Shift, L-Ctrl, L-Alt, Space, ↑, ↓, ←, →
    {0x0f, 0x5f, 0x7b, 0x41, 0x53, 0x54, 0x52, 0x24}, // R-Shift, _, {, A, S, T, R, $
    {CODE_NERD, 0x2b, 0x22, 0x5a, 0x44, 0x59, 0x45, 0x25}, // NERD, +, ", Z, D, Y, E, %
    {0x0d, 0x7d, 0x3a, 0x58, 0x46, 0x55, 0x57, 0x5e}, // Enter, }, :, X, F, U, W, ^
    {0x09, 0x7c, 0x3f, 0x43, 0x47, 0x49, 0x51, 0x26}, // Tab, |, ?, C, G, I, Q, &
    {0x1b, 0x7e, 0x3e, 0x56, 0x48, 0x4f, 0x21, 0x2a}, // Esc, ~, >, V, H, O, !, *
    {CODE_CAPS, 0x08, 0x3c, 0x42, 0x4a, 0x50, 0x40, 0x28}, // Caps, Del, <, B, J, P, @, (
    {CODE_2NDFN, CODE_INS, 0x4d, 0x4e, 0x4b, 0x4c, 0x23, 0x29}  // 2ndFn, Ins, M, N, K, L, #, )
}
};
// オリジナル文字コード
// 0xfe: Caps

char ioexp_sl2bl(char code) {
    if(0x61 <= code && code <= 0x7a) return code - 0x20;
    else return code;
}

char ioexp_bl2tl(char code) {
    switch(code) {
        case 0x27: // '
            return 0x22; // "
            break;
        case 0x2c: // ,
            return 0x3c; // <
            break;
        case 0x2d: // -
            return 0x5f; // _
            break;
        case 0x2e: // .
            return 0x3e; // >
            break;
        case 0x2f: // /
            return 0x3f; // ?
            break;
        case 0x30: // 0
            return 0x29; // )
            break;
        case 0x31: // 1
            return 0x21; // !
            break;
        case 0x32: // 2
            return 0x40; // @
            break;
        case 0x33: // 3
            return 0x23; // #
            break;
        case 0x34: // 4
            return 0x24; // $
            break;
        case 0x35: // 5
            return 0x25; // %
            break;
        case 0x36: // 6
            return 0x5e; // ^
            break;
        case 0x37: // 7
            return 0x26; // &
            break;
        case 0x38: // 8
            return 0x2a; // *
            break;
        case 0x39: // 9
            return 0x28; // (
            break;
        case 0x3b: // ;
            return 0x3a; // :
            break;
        case 0x3d: // =
            return 0x2b; // +
            break;
        case 0x5b: // [
            return 0x7b; // {
            break;
        case 0x5c: // 
            return 0x7c; // |
            break;
        case 0x5d: // ]
            return 0x7d; // }
            break;
        case 0x60: // `
            return 0x7e; // ~
            break;
    }
    return code;
}

char ioexp_getchr() {
    while(!flg_getchr_available);
    flg_getchr_available = false;
    return current_chr;
}

uint32_t ioexp_getchr_available() {
    if(flg_getchr_available) return 1;
    else return 0;
}

bool ioexp_repeating_timer_callback(struct repeating_timer *t) {

    uint8_t chrinfo[2] = {0};
    ioexp_getchrinfo(chrinfo);
    if(chrinfo[0] != 0x00) { // 未入力、未定義のキーは処理しない
        if(chrinfo[1] == button_push) {
            if(chrinfo[0] == 0x0e || chrinfo[0] == 0x0f) { // Shift
                status_shift = true;
            } else if(chrinfo[0] == CODE_CAPS) { // Caps
                status_caps = !status_caps;
            } else if(chrinfo[0] == CODE_2NDFN) { // 2ndFn
                status_2ndfn = !status_2ndfn;
            } else {
                current_chr = chrinfo[0];
                if(status_caps) current_chr = ioexp_sl2bl(current_chr);
                if(status_2ndfn) current_chr = ioexp_bl2tl(current_chr);
                flg_getchr_available = true;
            }
        } else {
            if(chrinfo[0] == 0x0e || chrinfo[0] == 0x0f) status_shift = false; // shift
        }
    }
    return true;
}

void ioexp_stop_keyscan_timer() {

    cancel_repeating_timer(&ioexp_timer);
}

void ioexp_start_keyscan_timer() {

    add_repeating_timer_ms(50, ioexp_repeating_timer_callback, NULL, &ioexp_timer);
}

void ioexp_init() {
    
    // リセット
    gpio_put(PIN_IOEXP_RST, 0);
    sleep_ms(60);
    gpio_put(PIN_IOEXP_RST, 1);
    sleep_ms(60);

    // 出力初期値を設定
    // 1 = High, 0 = Low
    ioexp_write_register(IOEXP_OLATA, 0b00000000);
    ioexp_write_register(IOEXP_OLATB, 0b11111111);

    // 入力論理を設定
    // 1 = 負論理, 0 = 正論理
    ioexp_write_register(IOEXP_IPOLA, 0b00000000);
    ioexp_write_register(IOEXP_IPOLB, 0b00000000);

    // 入力PullUpを設定
    // 1 = PullUp ON, 0 = PullUp OFF
    ioexp_write_register(IOEXP_GPPUA, 0b11111111);
    ioexp_write_register(IOEXP_GPPUB, 0b00000000);

    // 入出力方向を設定する
    // 1 = 入力, 0 = 出力
    ioexp_write_register(IOEXP_IODIRA, 0b11111111);
    ioexp_write_register(IOEXP_IODIRB, 0b00000000);

    ioexp_start_keyscan_timer();
}

void ioexp_write_register(uint8_t reg, uint8_t value) {

    uint8_t command[] = { reg, value };
    i2c_write_blocking(i2c0, IOEXP_ADDR, command, 2, false);
}

void ioexp_read_register(uint8_t reg, uint8_t retval[1]) {

    uint8_t command[] = { reg };
    i2c_write_blocking(i2c0, IOEXP_ADDR, command, 1, true);
    i2c_read_blocking(i2c0, IOEXP_ADDR, retval, 1, false);
}

void ioexp_getchrinfo(uint8_t chrinfo[2]) {

    chrinfo[0] = 0x00;

    for(int i=0; i<8; i++){

        uint8_t offbit = ~(0b00000001 << i);
        ioexp_write_register(IOEXP_OLATB, offbit);
        uint8_t current_keyinfo[1] = { 0xff };
        ioexp_read_register(IOEXP_GPIOA, current_keyinfo);

        if(prev_keyinfo[i] != current_keyinfo[0]) {
            for(int j=0; j<8; j++){
                uint8_t prev_keyinfo_tmp = prev_keyinfo[i];
                uint8_t current_keyinfo_tmp = current_keyinfo[0];
                prev_keyinfo_tmp <<= 7-j;
                current_keyinfo_tmp <<= 7-j;
                if(prev_keyinfo_tmp != current_keyinfo_tmp) {
                    chrinfo[0] = table_key2code[status_shift][j][i]; // 変化があったキーのコードを格納
                    if(!g_en_esc && chrinfo[0] == 0x1b) chrinfo[0] = 0; // ESCを無効にしていた場合
                    if(prev_keyinfo_tmp > current_keyinfo_tmp) {
                        chrinfo[1] = button_push;
                    } else {
                        chrinfo[1] = button_release;
                    }
                    //// スキャンしたところまでprev_keyinfoに格納
                    //uint8_t current_keyinfo_tmp = current_keyinfo[0];
                    //current_keyinfo_tmp = current_keyinfo_tmp << 7-i;
                    //current_keyinfo_tmp = current_keyinfo_tmp >> 7-i;
                    //uint8_t prev_keyinfo_tmp = prev_keyinfo[i];
                    //prev_keyinfo_tmp = prev_keyinfo_tmp >> i+1;
                    //prev_keyinfo_tmp = prev_keyinfo_tmp << i+1;
                    prev_keyinfo[i] = current_keyinfo[0];
                    return;
                }
            }
        }
    }
}

short ioexp_getkey(short index) {

    ioexp_stop_keyscan_timer();

    short keys[8] = {0x00};
    short index_current = 0;

    bool flg_shift = false;
    for(int i=0; i<8; i++){

        uint8_t offbit = ~(0b00000001 << i);
        ioexp_write_register(IOEXP_OLATB, offbit);
        uint8_t current_keyinfo[1] = { 0xff };
        ioexp_read_register(IOEXP_GPIOA, current_keyinfo);

        for(int j=0; j<8; j++){

            uint8_t prev_keyinfo_tmp = 0b11111111;
            uint8_t current_keyinfo_tmp = current_keyinfo[0];
            prev_keyinfo_tmp <<= 7-j;
            current_keyinfo_tmp <<= 7-j;
            if(prev_keyinfo_tmp != current_keyinfo_tmp) {
                // keyscanがストップしているのでフラグが立っていればShiftとEscを拾う
                if(g_en_shift && (table_key2code[status_shift][j][i] == 0x0e || table_key2code[status_shift][j][i] == 0x0f)) { // Shift
                    status_shift = true;
                    flg_shift = true;
                } else if (g_en_esc && table_key2code[status_shift][j][i] == 0x1b) { // Esc
                    current_chr = 0x1b;
                    flg_getchr_available = true;
                }
                if(g_en_shift) keys[index_current] = table_key2code[status_shift][j][i]; // 押下したキーのコードを格納
                else keys[index_current] = table_key2code[0][j][i];
                index_current++;
                break; // キーマトリクスの一列につき1つまで入力可
            }
        }
    }
    if(!flg_shift) status_shift = false;

    ioexp_start_keyscan_timer();

    return keys[index];
}
