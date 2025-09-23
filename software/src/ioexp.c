#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pin.h"
#include "ioexp.h"

static struct repeating_timer ioexp_timer;

// 前回スキャン時のキーの押下情報 1ビットで記録 0=押下 1=リリース
volatile uint8_t prev_keyinfo[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
volatile uint8_t current_chr_buf[IOEXP_CHRBUF] = { 0x00 };
// 読み出し位置のポインタ
volatile uint8_t current_chr_buf_rp = 0;
// 書き込み位置のポインタ
volatile uint8_t current_chr_buf_wp = 0;

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

void ioexp_write_register(uint8_t reg, uint8_t value) {

    uint8_t command[] = { reg, value };
    i2c_write_blocking(i2c0, IOEXP_ADDR, command, 2, false);
}

void ioexp_read_register(uint8_t reg, uint8_t retval[1]) {

    uint8_t command[] = { reg };
    i2c_write_blocking(i2c0, IOEXP_ADDR, command, 1, true);
    i2c_read_blocking(i2c0, IOEXP_ADDR, retval, 1, false);
}

void ioexp_current_chr_buf_write(uint8_t chr) {

  printf("write buf:%c\n", chr);
  current_chr_buf[current_chr_buf_wp] = chr;
  current_chr_buf_wp++;

  // リングバッファの最後尾に到達すると冒頭にポインタを移動
  if(current_chr_buf_wp == IOEXP_CHRBUF) current_chr_buf_wp = 0;
}

uint8_t ioexp_current_chr_buf_read() {

  uint8_t chr_return = current_chr_buf[current_chr_buf_rp];

  current_chr_buf_rp++;

  // リングバッファの最後尾に到達すると冒頭にポインタを移動
  if(current_chr_buf_rp == IOEXP_CHRBUF) current_chr_buf_rp = 0;

  return chr_return;
  
}

char ioexp_getchr() {
    while(current_chr_buf_wp == current_chr_buf_rp);
    return ioexp_current_chr_buf_read();
}

uint32_t ioexp_getchr_available() {
    if(current_chr_buf_wp != current_chr_buf_rp) return 1;
    else return 0;
}

void ioexp_stop_keyscan_timer() {

    cancel_repeating_timer(&ioexp_timer);

}

bool ioexp_repeating_timer_callback(struct repeating_timer *t) {

    ioexp_getchrinfo();
    ioexp_stop_keyscan_timer();
    return true;

}

void ioexp_start_keyscan_timer() {

    add_repeating_timer_ms(20, ioexp_repeating_timer_callback, NULL, &ioexp_timer);

}

void ioexp_reset_inta() {

  uint8_t tmp_keyinfo[1] = { 0xff };
  ioexp_read_register(IOEXP_GPIOA, tmp_keyinfo);

}


void ioexp_gpio_callback(uint gpio, uint32_t events) {

  if (gpio == PIN_IOEXP_INTA) {

    if (events & GPIO_IRQ_EDGE_FALL) {

      ioexp_reset_inta();
      ioexp_stop_keyscan_timer();
      ioexp_start_keyscan_timer();

    }

  }

}

void ioexp_start_keyscan_interrupt() {

  // 全ての入力を有効にして割り込みに備える
  ioexp_write_register(IOEXP_OLATB, 0b00000000);

  // INTAリセットのための読み出し
  ioexp_reset_inta();

  // ハードウェア割り込み再開
  gpio_set_irq_enabled_with_callback(
      PIN_IOEXP_INTA,
      GPIO_IRQ_EDGE_FALL,
      true,
      &ioexp_gpio_callback
      );

}

void ioexp_stop_keyscan_interrupt() {

    gpio_set_irq_enabled_with_callback(
        PIN_IOEXP_INTA,
        GPIO_IRQ_EDGE_FALL,
        false,
        &ioexp_gpio_callback
        );

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

    // Aピンの割り込みを許可する
    ioexp_write_register(IOEXP_GPINTENA, 0b11111111);

    // ハードウェア割り込みの有効化
    ioexp_start_keyscan_interrupt();

}

void ioexp_getchrinfo() {

  uint8_t chrinfo[2];

  // キースキャン中に発生する割り込みを無視
  ioexp_stop_keyscan_interrupt();

  chrinfo[0] = 0x00;

  // 1列ずつキーマトリクスで押下情報が変化した列を確認する
  for(int i=0; i<8; i++){

    // 対象の列の出力を変えてその列の情報を取得
    uint8_t offbit = ~(0b00000001 << i);
    ioexp_write_register(IOEXP_OLATB, offbit);
    uint8_t current_keyinfo[1] = { 0xff };
    ioexp_read_register(IOEXP_GPIOA, current_keyinfo);

    // 変化していたら
    if(prev_keyinfo[i] != current_keyinfo[0]) {

      // 1行ずつ確認
      for(int j=0; j<8; j++){

        // ビット演算のためにtmpに格納
        uint8_t prev_keyinfo_tmp = prev_keyinfo[i];
        uint8_t current_keyinfo_tmp = current_keyinfo[0];

        prev_keyinfo_tmp &= 1<<j;
        current_keyinfo_tmp &= 1<<j;

        if(prev_keyinfo_tmp != current_keyinfo_tmp) {

          // 変化があったキーのコードを格納
          chrinfo[0] = table_key2code[status_shift][j][i];

          // ESCを無効にしていた場合
          if(!g_en_esc && chrinfo[0] == 0x1b) chrinfo[0] = 0;

          // プッシュ、リリースの情報を格納
          if(prev_keyinfo_tmp > current_keyinfo_tmp) {
            chrinfo[1] = button_push;
          } else {
            chrinfo[1] = button_release;
          }

          // スキャンしたところまでprev_keyinfoと統合
          // j+1ビット分のマスク (例: j=1 -> 0b00000011)
          uint8_t mask = (1U << j+1) - 1;
          prev_keyinfo[i] = 
            (prev_keyinfo[i] & ~mask) | 
            (current_keyinfo[0] & mask);

          if(chrinfo[0] != 0x00) { // 未入力、未定義のキーは処理しない
            if(chrinfo[1] == button_push) {
              if(chrinfo[0] == 0x0e || chrinfo[0] == 0x0f) { // Shift
                status_shift = true;
              } else if(chrinfo[0] == CODE_CAPS) { // Caps
                status_caps = !status_caps;
              } else if(chrinfo[0] == CODE_2NDFN) { // 2ndFn
                status_2ndfn = !status_2ndfn;
              } else {
                uint8_t current_chr_tmp = chrinfo[0];
                if(status_caps) current_chr_tmp = ioexp_sl2bl(current_chr_tmp);
                if(status_2ndfn) current_chr_tmp = ioexp_bl2tl(current_chr_tmp);
                ioexp_current_chr_buf_write(current_chr_tmp);
              }
            } else {
              if(chrinfo[0] == 0x0e || chrinfo[0] == 0x0f) status_shift = false; // shift
            }
          }

        }
      }
    }
  }

  ioexp_start_keyscan_interrupt();

}

short ioexp_getkey(short index) {

    ioexp_stop_keyscan_interrupt();

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
                    ioexp_current_chr_buf_write(0x1b);
                }
                if(g_en_shift) keys[index_current] = table_key2code[status_shift][j][i]; // 押下したキーのコードを格納
                else keys[index_current] = table_key2code[0][j][i];
                index_current++;
                break; // キーマトリクスの一列につき1つまで入力可
            }
        }
    }
    if(!flg_shift) status_shift = false;

    ioexp_start_keyscan_interrupt();

    return keys[index];
}
