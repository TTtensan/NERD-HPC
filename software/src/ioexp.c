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
// I2Cが連続して失敗した回数。1回成功すると0に戻る
static volatile uint8_t ioexp_fail_count = 0;
// 次に復旧を試みる時刻[us]
static uint32_t ioexp_next_recover = 0;
// バスが全く応答しない状態が何回続いたか
static uint8_t ioexp_dead_count = 0;
// 何回スキャンしたら設定レジスタの健全性を確認するかのカウンタ
static uint16_t ioexp_health_count = 0;
// 全てのキーがリリース済みで、かつその状態が確定していることが分かっている。
// このときは8列スキャンを省略できる
static bool ioexp_all_released = false;

// MCP23017の設定が生きていることを確認できている。
// falseの間はキー入力を一切生成しない。
// 設定が飛んだMCP23017は、行のプルアップ(GPPUA)も列の出力(IODIRB)も
// 初期値に戻っているため、キーマトリクスを読むと浮いた固定パターンが返り、
// 全ての列で同じ行が押されたように見える。
// これをそのまま流すと大量のゴーストキーになる
static volatile bool ioexp_healthy = false;

static void ioexp_restore_config();
static bool ioexp_is_healthy();
static void ioexp_dump_registers(const char *why, const uint8_t scan[8]);

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
//
// また、単発の失敗では復旧まで踏み込まない。
// 復旧はMCP23017のハードウェアリセットを伴い、その前後の一瞬は
// GPPUA=0x00 (行のプルアップ無し) / IODIRB=0xff (列が入力) という
// 初期値の状態になる。この窓でキーマトリクスを読むと行が浮いた値を拾う。
// I2C0は1MHzで動いており単発のNAKは起こり得るので、
// 1回の失敗で毎回リセットしていると、その窓を頻繁に作ってしまう。
// 戻り値: 0 = 成功, -1 = 失敗

static void ioexp_i2c_failed() {

    if(ioexp_fail_count < IOEXP_FAIL_THRESHOLD) ioexp_fail_count++;

    // 転送が失敗した時点で、バイト列がずれてレジスタが化けている可能性がある。
    // 定期確認(500ms間隔)を待たず、次のスキャンで設定を検査させる
    ioexp_health_count = IOEXP_HEALTH_CHECK_SCANS;

#if IOEXP_DEBUG_LOG_I2C_FAIL
    // 転送の失敗が実際に起きているかを直接観測するため。
    // 出力は1秒に1回までに絞り、その間の回数は積算値で分かるようにする
    static uint32_t fail_total = 0;
    static uint32_t last_log = 0;
    fail_total++;
    if(last_log == 0 || (int32_t)(time_us_32() - last_log) >= 1000000) {
        last_log = time_us_32();
        if(last_log == 0) last_log = 1;
        printf("[ioexp] i2c fail total=%lu consecutive=%u timeout=%dus\n",
               (unsigned long)fail_total,
               (unsigned)ioexp_fail_count,
               (int)IOEXP_I2C_TIMEOUT_US);
    }
#endif

    // 連続して失敗したときだけ、バスが本当に壊れたと判断する
    if(ioexp_fail_count >= IOEXP_FAIL_THRESHOLD) ioexp_need_recover = true;

}

int ioexp_write_register(uint8_t reg, uint8_t value) {

    uint8_t command[] = { reg, value };

    int ret = i2c_write_timeout_us(i2c0, IOEXP_ADDR, command, 2, false, IOEXP_I2C_TIMEOUT_US);
    if (ret != 2) {
        ioexp_i2c_failed();
        return -1;
    }

    ioexp_fail_count = 0;
    return 0;
}

int ioexp_read_register(uint8_t reg, uint8_t retval[1]) {

    uint8_t command[] = { reg };

    int ret = i2c_write_timeout_us(i2c0, IOEXP_ADDR, command, 1, true, IOEXP_I2C_TIMEOUT_US);
    if (ret != 1) {
        ioexp_i2c_failed();
        return -1;
    }

    ret = i2c_read_timeout_us(i2c0, IOEXP_ADDR, retval, 1, false, IOEXP_I2C_TIMEOUT_US);
    if (ret != 1) {
        ioexp_i2c_failed();
        return -1;
    }

    ioexp_fail_count = 0;
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

#if IOEXP_DEBUG_FORCE_BANK1
    // 【検証用】起動から一定時間経ったら一度だけIOCON.BANKを立てて、
    // レジスタマップがずれた状態を作る。
    // これ以降、0x15はOLATBではなくIOCONを指すので、
    // キースキャンの列選択がそのままIOCONへの書き込みになる
    static bool debug_bank1_done = false;
    if(!debug_bank1_done &&
       time_us_32() >= (uint32_t)IOEXP_DEBUG_FORCE_BANK1_DELAY_MS * 1000) {

        debug_bank1_done = true;
        printf("[ioexp] DEBUG: forcing IOCON.BANK=1\n");
        ioexp_write_register(IOEXP_IOCON, 0b10000000);

        // 省略経路はOLATBに0x00を書く。
        // BANK=1ではこれがIOCON=0x00となり、BANKが即座に戻ってしまうので、
        // 全列スキャンを通るようにしておく
        ioexp_all_released = false;
        return;

    }
#endif

    // 設定が飛んでいる間はスキャンせず、設定の書き戻しだけを試みる。
    // ただし毎スキャン試してはいけない。
    // バスが固着していると1トランザクションあたりIOEXP_I2C_TIMEOUT_US
    // 待たされるので、10ms間隔で試すとCPUを食い潰して端末全体が固まる
    if(!ioexp_healthy || ioexp_need_recover) {
        if((int32_t)(time_us_32() - ioexp_next_recover) < 0) return;
        ioexp_next_recover = time_us_32() + IOEXP_RECOVER_INTERVAL_MS * 1000;
        ioexp_restore_config();
        return;
    }

    // 定期的にMCP23017の設定が生きているか確認する。
    // I2Cはエラーを返さないのにキーだけ効かなくなる状態を検出するため
    if(++ioexp_health_count >= IOEXP_HEALTH_CHECK_SCANS) {
        ioexp_health_count = 0;
        if(!ioexp_is_healthy()) {
            ioexp_dump_registers("config-lost", NULL);
            ioexp_healthy = false;
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
    sleep_ms(60);
    gpio_put(PIN_IOEXP_RST, 1);
    sleep_ms(60);

}

// MCP23017の設定レジスタを書き込む。
// 初期化時だけでなく、外乱でレジスタが化けた場合の復旧でも使う
static void ioexp_config_registers() {

    // 何よりも先にIOCONのBANKを0へ戻す。
    // BANK=1になるとレジスタマップ全体がずれ、
    // 0x15がOLATBではなくIOCONを指すようになる。
    // そうなるとキースキャンの列選択が毎回IOCONへの書き込みになり、
    // ドライバ自身がIOCONを壊し続けて電源を切るまで復帰しなくなる。
    //
    // BANK=1のとき IOCON は 0x05、BANK=0のとき 0x0a に居る。
    // 現在どちらか分からないので両方へ0を書く。
    // BANK=0のときの0x05はGPINTENBで、0を書くのは意図通り(B側の割り込みは使わない)
#if !IOEXP_DEBUG_NO_IOCON_INIT
    ioexp_write_register(IOEXP_IOCON_BANK1, 0b00000000);
    ioexp_write_register(IOEXP_IOCON, 0b00000000);
#endif

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
        sleep_us(IOEXP_SETTLE_US); // 行が立ち上がるのを待つ
        uint8_t current_keyinfo[1] = { 0xff };
        if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) continue;

        prev_keyinfo[i] = current_keyinfo[0];
        sample_keyinfo[i] = current_keyinfo[0];

    }

}

// 設定が飛んだMCP23017を書き戻す。
//
// ここでハードウェアリセット(ioexp_hard_reset)やバスのビットバンギング
// (i2c_bus_recover)は行わない。どちらも実行中はMCP23017が初期値の状態になり、
// その窓でキーマトリクスを読むとゴーストキーが出る。
// さらに書き戻しに失敗すると初期値のまま放置され、
// 「設定が飛んでいるので復旧 -> 復旧が失敗してまた飛んだまま」という
// 自己持続的なループに陥る。実際にこれで大量のゴーストキーが出た。
//
// 設定レジスタへの書き込みは何度実行しても副作用が無いので、
// 成功するまで毎スキャン静かに繰り返せばよい。
// 書き戻せたことを読み返しで確認できるまで、キー入力は一切生成しない。
static void ioexp_restore_config() {

    ioexp_need_recover = false;
    ioexp_healthy = false;

    // まず1トランザクションだけ投げて、バスが生きているか安く確かめる。
    // 死んでいるバスに設定の書き戻し(11回)と検査(4回)を投げると、
    // 全部タイムアウトして1回の呼び出しで数百ms失う
    uint8_t probe[1] = { 0x00 };
    if(ioexp_read_register(IOEXP_IODIRA, probe) < 0) {

        if(ioexp_dead_count < IOEXP_BUS_RECOVER_AFTER) ioexp_dead_count++;

        // 一定時間応答が無ければ、バスがスレーブ側でLowに固着していると判断する。
        // ここまで来ないとSCLを叩かないのは、
        // 単発の失敗でバスを触ると、かえってMCP23017のレジスタを壊すため
        if(ioexp_dead_count >= IOEXP_BUS_RECOVER_AFTER) {

            ioexp_dead_count = 0;
            ioexp_dump_registers("bus-dead", NULL);

            // SCLを9回叩いてスレーブに残りのビットを吐かせ、STOPで解放する
            i2c_bus_recover(PIN_IOEXP_SDA, PIN_IOEXP_SCL);

            // それでも解放されない場合に備えてチップごとリセットする。
            // この直後は設定が初期値なので、
            // 下の検査を通るまでキー入力は生成されない
            ioexp_hard_reset();

        }

        return;

    }

    ioexp_dead_count = 0;

    ioexp_config_registers();

    // 書き戻せたか読み返して確認する。
    // 確認できるまでキー入力は止めたままにする
    if(!ioexp_is_healthy()) {
        // ここを黙って抜けると、設定を書き戻せない状態が
        // 何のログも出さずに続いてしまう
        ioexp_dump_registers("restore-failed", NULL);
        return;
    }

    ioexp_resync_keyinfo();
    ioexp_healthy = true;

}

// 異常を検出したときにMCP23017の全レジスタをUSBシリアルへ吐く。
// 原因の切り分け用。
// BANK=0のマップで 0x00-0x15 を、加えてBANK=1で意味を持つ 0x16-0x1a を読む。
//
// 読み方:
// - 0x00(IODIRA)=ff / 0x01(IODIRB)=00 / 0x0c(GPPUA)=ff なら設定は生きている
// - 0x0a(IOCON)のbit7が1、または 0x0c が ff 以外なら BANK がずれている
// - scan= の8バイトが全て同じなら、列選択が行に効いていない
static void ioexp_dump_registers(const char *why, const uint8_t scan[8]) {

#if !IOEXP_DEBUG_DUMP

    (void)why;
    (void)scan;

#else

    // USBシリアルへの出力は遅いので、1秒に1回までに絞る
    static uint32_t last_dump = 0;
    if(last_dump != 0 && (int32_t)(time_us_32() - last_dump) < 1000000) return;
    last_dump = time_us_32();
    if(last_dump == 0) last_dump = 1;

    printf("[ioexp] %s scan=", why);
    if(scan == NULL) printf("---- ");
    else for(int i=0; i<8; i++) printf("%02x ", scan[i]);

    printf("| reg=");
    for(uint8_t reg=0x00; reg<=0x1a; reg++) {
        uint8_t val[1] = { 0x00 };
        if(ioexp_read_register(reg, val) < 0) {
            // バスが無応答なら全部読めない。
            // 27回分のタイムアウトを待つと、それだけで
            // IOEXP_I2C_TIMEOUT_US * 27 (約0.8秒) 固まってしまう。
            // 診断のためのコードで固めては本末転倒なので、ここで打ち切る
            printf("%02x:-- (no response, aborted)", reg);
            break;
        }
        printf("%02x:%02x ", reg, val[0]);
    }
    printf("\n");

#endif

}

// MCP23017の設定が生きているか確認する。
// RSTピンやI2Cピンに外部から触れると、MCP23017だけがリセット/化けを起こす。
// このときMCU側から見るとI2Cは正常にACKを返し続けるのでエラーにならず、
// 「エラーは出ないがキーだけ永久に効かない」状態になる。
// IODIRBは設定値が0x00、リセット後の初期値が0xffなので、これで判別できる
static bool ioexp_is_healthy() {

    uint8_t val[1] = { 0xff };

    // 読めなかった場合も「確認できていない」とみなす。
    // 設定の書き戻しはハードウェアリセットを伴わず何度でも安全に行えるので、
    // 単発のNAKで一時的にfalseになっても、次のスキャンで静かに復帰する

    // IODIRAは必ず確認すること。
    // 転送がずれるとこのレジスタだけが 0xff -> 0x00 に化けることがあり、
    // その場合MCP23017が行を出力として全部Lowに叩くため、
    // 全てのキーが押されっぱなしに見える。
    // 他のレジスタは後続の書き込みで上書きされて正常値に戻るので、
    // ここを見ないと壊れていることに気付けない
    if(ioexp_read_register(IOEXP_IODIRA, val) < 0) return false;
    if(val[0] != 0b11111111) return false;

    if(ioexp_read_register(IOEXP_IODIRB, val) < 0) return false;
    if(val[0] != 0b00000000) return false;

    // 行のプルアップ。これが落ちると行が浮く
    if(ioexp_read_register(IOEXP_GPPUA, val) < 0) return false;
    if(val[0] != 0b11111111) return false;

    // レジスタマップ自体がずれていないことの確認
    if(ioexp_read_register(IOEXP_IOCON, val) < 0) return false;

    return (val[0] == 0b00000000);

}

void ioexp_init() {

    // 電源投入直後だけはハードウェアリセットをかける。
    // 動作中の復旧では使わない (ioexp_restore_config のコメント参照)
    ioexp_hard_reset();
    ioexp_config_registers();

    if(ioexp_is_healthy()) {
        ioexp_resync_keyinfo();
        ioexp_healthy = true;
    }

    // キースキャンの有効化。
    // 設定に失敗していても、ioexp_task()が書き戻しを繰り返して復帰する
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
    sleep_us(IOEXP_SETTLE_US); // 行が立ち上がるのを待つ
    uint8_t any_keyinfo[1] = { 0xff };
    if(ioexp_read_register(IOEXP_GPIOA, any_keyinfo) < 0) return;

    // 全列をLowにしたまま放置しない。
    // 放置すると、次にioexp_getkey()が呼ばれたときに
    // 7列が同時にLow->Highへ遷移した直後の行を読むことになり、
    // 行の立ち上がりが間に合わずゴーストキーになる。
    // ioexp_getkey()はデバウンスを持たないので、これがそのまま入力として出る
    if(ioexp_write_register(IOEXP_OLATB, 0b11111111) < 0) return;

    // まだ全リリースのままなら、列ごとの状態は変化しようがない
    if(any_keyinfo[0] == 0xff) return;

  }

  // ここから先は列ごとの状態を更新する。
  // 途中でreturnした場合に省略経路へ入らないよう、先に倒しておく
  ioexp_all_released = false;

  // 先に8列分を読み切る。
  // 判定の前に全列が揃っていれば、読み値の妥当性を検査できる
  uint8_t scan[8];
  for(int i=0; i<8; i++){

    // 対象の列の出力を変えてその列の情報を取得
    // 1回でも失敗したらこの回のスキャンは捨てる。
    // 化けた読み値から差分を作らないためと、
    // 死んだバスに残り7列分の通信を投げて時間を捨てないため
    uint8_t offbit = ~(0b00000001 << i);
    if(ioexp_write_register(IOEXP_OLATB, offbit) < 0) return;
    sleep_us(IOEXP_SETTLE_US); // 行が立ち上がるのを待つ
    uint8_t current_keyinfo[1] = { 0xff };
    if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) return;
    scan[i] = current_keyinfo[0];

  }

  // 列の出力を変えたのに読み値が1つも変わらないなら、
  // 読み値はキーマトリクスの状態を表していない。
  // 実際にGPIOAの読み出しが直前に書いたOLATBのレジスタアドレス(0x15)を
  // 返し続け、全ての列で同じ行が押されたように見える不具合が起きた。
  // これを通すと、Shift/Enter/Capsを含む大量のゴーストキーが
  // 延々とバッファに流れ込む。
  // 押下ビットが立った状態でこれが起きたらスキャンごと捨て、
  // 設定の確認からやり直す
  bool all_same = true;
  for(int i=1; i<8; i++) {
    if(scan[i] != scan[0]) { all_same = false; break; }
  }
  if(all_same && scan[0] != 0xff) {
    ioexp_dump_registers("same-on-all-columns", scan);
    ioexp_healthy = false;
    return;
  }

  // 1列ずつキーマトリクスで押下情報が変化した列を確認する
  for(int i=0; i<8; i++){

    // デバウンス。
    // 前回のスキャンと読み値が違う間は確定させない。
    // 2回連続で同じ値が読めて初めて下の判定に進む
    if(sample_keyinfo[i] != scan[i]) {
      sample_keyinfo[i] = scan[i];
      continue;
    }

    // 変化していたら
    if(prev_keyinfo[i] != scan[i]) {

      // 1行ずつ確認
      for(int j=0; j<8; j++){

        // ビット演算のためにtmpに格納
        uint8_t prev_keyinfo_tmp = prev_keyinfo[i];
        uint8_t current_keyinfo_tmp = scan[i];

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
      prev_keyinfo[i] = scan[i];

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
    // 設定が飛んでいる間は行が浮いた値を拾うので、キー無しとして返す。
    // ioexp_getkey()はデバウンスを持たないため、ここを通すと
    // 浮いた値がそのままキーコードとして返る
    if(!ioexp_healthy) return 0;

    bool scan_was_enabled = ioexp_scan_enable;
    ioexp_scan_enable = false;

    short keys[8] = {0x00};
    short index_current = 0;

    bool flg_shift = false;
    bool i2c_failed = false;

    // 先に8列分を読み切ってから判定する (ioexp_getchrinfo と同じ理由)
    uint8_t scan[8];
    for(int i=0; i<8; i++){

        // 1列でも読めなかったら結果ごと捨てる。
        // ここでcontinueすると、読めた列の結果だけで判定してしまう
        uint8_t offbit = ~(0b00000001 << i);
        if(ioexp_write_register(IOEXP_OLATB, offbit) < 0) { i2c_failed = true; break; }
        sleep_us(IOEXP_SETTLE_US); // 行が立ち上がるのを待つ
        uint8_t current_keyinfo[1] = { 0xff };
        if(ioexp_read_register(IOEXP_GPIOA, current_keyinfo) < 0) { i2c_failed = true; break; }
        scan[i] = current_keyinfo[0];

    }

    // 列を変えても読み値が変わらないなら、読み値が信用できない。
    // ioexp_getkey()はデバウンスを持たないので特に危険
    if(!i2c_failed) {
        bool all_same = true;
        for(int i=1; i<8; i++) {
            if(scan[i] != scan[0]) { all_same = false; break; }
        }
        if(all_same && scan[0] != 0xff) {
            ioexp_dump_registers("same-on-all-columns", scan);
            ioexp_healthy = false;
            i2c_failed = true;
        }
    }

    if(!i2c_failed) for(int i=0; i<8; i++){

        for(int j=0; j<8; j++){

            uint8_t prev_keyinfo_tmp = 0b11111111;
            uint8_t current_keyinfo_tmp = scan[i];
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
    // prev_keyinfo / sample_keyinfo はあえて触らない。
    // 停止中に押されたキーは、再開後に差分として正しく拾われる
    ioexp_scan_enable = scan_was_enabled;
    ioexp_next_scan = time_us_32() + IOEXP_SCAN_INTERVAL_MS * 1000;

    // 読めなかった場合はstatus_shiftも更新せず、キー無しとして返す
    if(i2c_failed) return 0;

    if(!flg_shift) status_shift = false;

    return keys[index];
}
