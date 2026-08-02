#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pin.h"
#include "ioexp.h"

// 次にキースキャンを行う時刻[us]。
// get_absolute_time()は64bitタイマをTIMELR->TIMEHRの順に読む
// (桁上がり対策のリトライループ付き) ので1回30〜50サイクルかかる。
// ioexp_task()はBASICの中間コード1個ごとに呼ばれるため、
// TIMERAWLを1回読むだけのtime_us_32()を使う。
// 32bitは約71分でラップするが、符号付きの差で比較すれば正しく判定できる
static uint32_t ioexp_next_scan = 0;
// キースキャンの一時停止フラグ (ioexp_getkey中にスキャンが割り込まないようにする)
static volatile bool ioexp_scan_enable = false;
// I2Cに失敗した。次のioexp_task()で復旧を試みる
static volatile bool ioexp_need_recover = false;
// 何回スキャンしたら設定レジスタの健全性を確認するかのカウンタ
static uint16_t ioexp_health_count = 0;
// 全てのキーがリリース済みで、かつその状態が確定していることが分かっている。
// このときは8列スキャンを省略できる
static bool ioexp_all_released = false;

static void ioexp_recover();
static bool ioexp_is_healthy();

// 前回スキャン時のキーの押下情報 1ビットで記録 0=押下 1=リリース
volatile uint8_t prev_keyinfo[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
// デバウンス用。前回スキャンの生の読み値。2回連続で一致して初めて確定させる
static uint8_t sample_keyinfo[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
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

// I2Cはオープンドレインのバスなので、Lowは「出力にして0を出す」、
// Highは「入力に戻してプルアップに任せる」で作る。
// 旧実装は gpio_set_dir(OUT) + gpio_put(1) でHighを能動的に driveしていた。
// これだとMCU側とMCP23017側が同時にバスをdriveして衝突し、
// 中途半端なビットがMCP23017に書き込まれてレジスタが化ける。
// これが「リカバリすると意図しないキー入力が出る」原因だった。
#define IOEXP_BUS_LOW(pin)     gpio_set_dir((pin), GPIO_OUT)
#define IOEXP_BUS_RELEASE(pin) gpio_set_dir((pin), GPIO_IN)

static bool i2c_bus_recover(uint sda_pin, uint scl_pin) {

    // 出力値は常にLow固定。開放はdirで行うのでgpio_put(1)は絶対にしない
    gpio_put(sda_pin, 0);
    gpio_put(scl_pin, 0);
    IOEXP_BUS_RELEASE(sda_pin);
    IOEXP_BUS_RELEASE(scl_pin);

    // I2C を GPIO に切り替え
    gpio_set_function(sda_pin, GPIO_FUNC_SIO);
    gpio_set_function(scl_pin, GPIO_FUNC_SIO);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    // SDA が Low のままならクロックを送ってスレーブに残りのビットを吐かせる
    for (int i = 0; i < 9; i++) {
        if (gpio_get(sda_pin)) break; // SDA が High ならOK
        IOEXP_BUS_LOW(scl_pin);
        sleep_us(10);
        IOEXP_BUS_RELEASE(scl_pin);
        sleep_us(10);
    }

    // STOP コンディションを生成 (SCLがHighの間にSDAをLow->High)
    IOEXP_BUS_LOW(sda_pin);
    sleep_us(10);
    IOEXP_BUS_RELEASE(scl_pin);
    sleep_us(10);
    IOEXP_BUS_RELEASE(sda_pin);
    sleep_us(10);

    bool bus_free = gpio_get(sda_pin) && gpio_get(scl_pin);

    // I2C 機能に戻す
    i2c_init(i2c0, I2C0_BAUDRATE); // 再初期化
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);

    return bus_free;
}

// 通信に失敗しても、その場ではリカバリしない。
// 「その回のスキャンを捨てて」フラグだけ立て、復旧は ioexp_task() が
// スキャンの境界で行う。転送の途中で状態を作り替えないことと、
// 復旧後に必ずキー状態を取り直すことで、ゴーストキーの発生を防ぐ。
// 戻り値: 0 = 成功, -1 = 失敗

int ioexp_write_register(uint8_t reg, uint8_t value) {

    uint8_t command[] = { reg, value };

    int ret = i2c_write_timeout_us(i2c0, IOEXP_ADDR, command, 2, false, IOEXP_I2C_TIMEOUT_US);
    if (ret != 2) {
        ioexp_need_recover = true;
        return -1;
    }

    return 0;
}

int ioexp_read_register(uint8_t reg, uint8_t retval[1]) {

    uint8_t command[] = { reg };

    int ret = i2c_write_timeout_us(i2c0, IOEXP_ADDR, command, 1, true, IOEXP_I2C_TIMEOUT_US);
    if (ret != 1) {
        ioexp_need_recover = true;
        return -1;
    }

    ret = i2c_read_timeout_us(i2c0, IOEXP_ADDR, retval, 1, false, IOEXP_I2C_TIMEOUT_US);
    if (ret != 1) {
        ioexp_need_recover = true;
        return -1;
    }

    return 0;
}

void ioexp_current_chr_buf_write(uint8_t chr) {

  // リングバッファの最後尾に到達すると冒頭にポインタを移動
  uint8_t next_wp = current_chr_buf_wp + 1;
  if(next_wp == IOEXP_CHRBUF) next_wp = 0;

  // 満杯なら捨てる。
  // ここで捨てないと wp が rp を追い越し、
  // wp == rp すなわちバッファが空の状態になって
  // 溜まっていた入力が全て消える
  if(next_wp == current_chr_buf_rp) return;

  // wpを進める前に中身を書く (読み出し側から見て中途半端な状態を作らない)
  current_chr_buf[current_chr_buf_wp] = chr;
  current_chr_buf_wp = next_wp;
}

uint8_t ioexp_current_chr_buf_read() {

  uint8_t chr_return = current_chr_buf[current_chr_buf_rp];

  current_chr_buf_rp++;

  // リングバッファの最後尾に到達すると冒頭にポインタを移動
  if(current_chr_buf_rp == IOEXP_CHRBUF) current_chr_buf_rp = 0;

  return chr_return;
  
}

char ioexp_getchr() {
    // バッファが空の間もスキャンを回し続ける。
    // ここで単純にスピンすると、スキャンが止まった瞬間に永久に抜けられなくなる
    while(current_chr_buf_wp == current_chr_buf_rp) ioexp_task();
    return ioexp_current_chr_buf_read();
}

uint32_t ioexp_getchr_available() {
    ioexp_task();
    if(current_chr_buf_wp != current_chr_buf_rp) return 1;
    else return 0;
}

// キースキャンの本体。
// 以前はrepeating_timerのコールバック (= 割り込みコンテキスト) から
// ioexp_getchrinfo() を直接呼んでいたが、1回のスキャンでI2Cを16回叩くため、
// バスが不調になると割り込みの中でCPUを数百ms占有して端末全体が固まっていた。
// 現在は割り込みを使わず、メインループから呼ばれたときだけスキャンする。
void ioexp_task() {

    if(!ioexp_scan_enable) return;

    // 前回のスキャンからIOEXP_SCAN_INTERVAL_MS経っていなければ何もしない
    if((int32_t)(time_us_32() - ioexp_next_scan) < 0) return;
    ioexp_next_scan = time_us_32() + IOEXP_SCAN_INTERVAL_MS * 1000;

    // 前回のスキャンでI2Cに失敗していたら、まず復旧を試みる
    if(ioexp_need_recover) {
        ioexp_recover();
        return;
    }

    // 定期的にMCP23017の設定が生きているか確認する。
    // I2Cはエラーを返さないのにキーだけ効かなくなる状態から復帰するため
    if(++ioexp_health_count >= IOEXP_HEALTH_CHECK_SCANS) {
        ioexp_health_count = 0;
        if(!ioexp_is_healthy()) {
            ioexp_recover();
            return;
        }
    }

    ioexp_getchrinfo();

}

void ioexp_stop_keyscan_timer() {

    ioexp_scan_enable = false;

}

void ioexp_start_keyscan_timer() {

    ioexp_next_scan = time_us_32();
    ioexp_scan_enable = true;

}

void ioexp_reset_inta() {

  uint8_t tmp_keyinfo[1] = { 0xff };
  ioexp_read_register(IOEXP_GPIOA, tmp_keyinfo);

}


void ioexp_gpio_callback(uint gpio, uint32_t events) {

  if (gpio == PIN_IOEXP_INTA) {

    if (events & GPIO_IRQ_EDGE_FALL) {

      // 割り込みコンテキストなのでI2Cは絶対に叩かない。
      // 次のioexp_task()を即座に実行させるだけにする。
      // INTAのクリア (GPIOAの読み出し) はスキャン側で行われる
      ioexp_next_scan = time_us_32();

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


// MCP23017をハードウェアリセットする。
// バスがスレーブ側でLowに固着している場合も、これで確実に解放される
static void ioexp_hard_reset() {

    gpio_put(PIN_IOEXP_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_IOEXP_RST, 1);
    sleep_ms(10);

}

// MCP23017の設定レジスタを書き込む。
// 初期化時だけでなく、外乱でレジスタが化けた場合の復旧でも使う
static void ioexp_config_registers() {

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

}

// 今のキーマトリクスの状態を読んで、差分判定の基準を作り直す。
// 押下/リリースのイベントは一切生成しない。
// 復旧の直後にこれをやらないと、復旧前後の状態差が全部
// キー入力として吐き出されてしまう
static void ioexp_resync_keyinfo() {

    // 読めなかった列があるかもしれないので、
    // 一旦「省略できない」側に倒しておく
    ioexp_all_released = false;

    for(int i=0; i<8; i++) {

        uint8_t offbit = ~(0b00000001 << i);
        if(ioexp_write_register(IOEXP_OLATB, offbit) < 0) continue;
        uint8_t current_keyinfo[1] = { 0xff };
        if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) continue;

        prev_keyinfo[i] = current_keyinfo[0];
        sample_keyinfo[i] = current_keyinfo[0];

    }

}

// I2Cが失敗した、または設定レジスタが化けていたときの復旧処理。
// バス解放 -> ハードリセット -> 設定書き直し -> キー状態の取り直し
static void ioexp_recover() {

    ioexp_need_recover = false;

    i2c_bus_recover(PIN_IOEXP_SDA, PIN_IOEXP_SCL);
    ioexp_hard_reset();
    ioexp_config_registers();

    // ここで失敗したなら復旧できていない。次のスキャンでまた試す
    if(ioexp_need_recover) return;

    ioexp_resync_keyinfo();

}

// MCP23017の設定が生きているか確認する。
// RSTピンやI2Cピンに外部から触れると、MCP23017だけがリセット/化けを起こす。
// このときMCU側から見るとI2Cは正常にACKを返し続けるのでエラーにならず、
// 「エラーは出ないがキーだけ永久に効かない」状態になる。
// IODIRBは設定値が0x00、リセット後の初期値が0xffなので、これで判別できる
static bool ioexp_is_healthy() {

    uint8_t iodirb[1] = { 0xff };
    if(ioexp_read_register(IOEXP_IODIRB, iodirb) < 0) return false;

    return (iodirb[0] == 0b00000000);

}

void ioexp_init() {

    ioexp_hard_reset();
    ioexp_config_registers();
    ioexp_resync_keyinfo();

    // キースキャンの有効化
    ioexp_start_keyscan_timer();

}

void ioexp_getchrinfo() {

  uint8_t chrinfo[2];

  chrinfo[0] = 0x00;

  // 全列を同時にLowにして1回だけ読み、どこかに押下があるかを先に調べる。
  // 何も押されていない状態が続く限り (BASIC実行中のほとんどの時間がこれ)
  // 8列スキャンを丸ごと省略できるので、1回のスキャンが
  // I2C 16回 (約0.7ms) から 2回 (約0.09ms) で済む
  if(ioexp_all_released) {

    if(ioexp_write_register(IOEXP_OLATB, 0b00000000) < 0) return;
    uint8_t any_keyinfo[1] = { 0xff };
    if(ioexp_read_register(IOEXP_GPIOA, any_keyinfo) < 0) return;

    // まだ全リリースのままなら、列ごとの状態は変化しようがない
    if(any_keyinfo[0] == 0xff) return;

  }

  // ここから先は列ごとの状態を更新する。
  // 途中でreturnした場合に省略経路へ入らないよう、先に倒しておく
  ioexp_all_released = false;

  // 1列ずつキーマトリクスで押下情報が変化した列を確認する
  for(int i=0; i<8; i++){

    // 対象の列の出力を変えてその列の情報を取得
    // 1回でも失敗したらこの回のスキャンは捨てる。
    // 化けた読み値から差分を作らないためと、
    // 死んだバスに残り7列分の通信を投げて時間を捨てないため
    uint8_t offbit = ~(0b00000001 << i);
    if(ioexp_write_register(IOEXP_OLATB, offbit) < 0) return;
    uint8_t current_keyinfo[1] = { 0xff };
    if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) return;

    // デバウンス。
    // 前回のスキャンと読み値が違う間は確定させない。
    // 2回連続で同じ値が読めて初めて下の判定に進む
    if(sample_keyinfo[i] != current_keyinfo[0]) {
      sample_keyinfo[i] = current_keyinfo[0];
      continue;
    }

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

          // キーの入力情報を変換してバッファに保存
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

      // prev_keyinfoを更新
      prev_keyinfo[i] = current_keyinfo[0];

    }
  }

  // 全列を最後まで読めて、かつ確定値・生値ともに全リリースなら、
  // 次回から省略経路を使える
  for(int i=0; i<8; i++) {
    if(prev_keyinfo[i] != 0xff || sample_keyinfo[i] != 0xff) return;
  }
  ioexp_all_released = true;

}

short ioexp_getkey(short index) {

    // BASIC側から任意の値が渡ってくるので範囲を検査する。
    // 検査しないとkeys[]の範囲外を読んでしまう
    if(index < 0 || index >= 8) return 0;

    // スキャンを一時停止する。
    // ここでI2Cの最中にスキャンが割り込むとバスの状態が壊れる
    bool scan_was_enabled = ioexp_scan_enable;
    ioexp_scan_enable = false;

    short keys[8] = {0x00};
    short index_current = 0;

    bool flg_shift = false;
    for(int i=0; i<8; i++){

        uint8_t offbit = ~(0b00000001 << i);
        if(ioexp_write_register(IOEXP_OLATB, offbit) < 0) continue;
        uint8_t current_keyinfo[1] = { 0xff };
        if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) continue;

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

    // prev_keyinfo / sample_keyinfo はあえて触らない。
    // 停止中に押されたキーは、再開後に差分として正しく拾われる
    ioexp_scan_enable = scan_was_enabled;
    ioexp_next_scan = time_us_32() + IOEXP_SCAN_INTERVAL_MS * 1000;

    return keys[index];
}
