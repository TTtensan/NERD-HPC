#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "lcd.h"
#include "pin.h"
#include "hardware/spi.h"
#include "font.h"
#include "sd.hpp"

// 8ビットの塊が縦方向に6個、横方向に128個あるイメージ、上のドットは下位ビット
volatile uint8_t v_buf[8][128];
// グラフィック画面用バッファ
volatile uint8_t vg_buf[8][128];

// SCR用の画面上文字バッファ
volatile uint8_t scr_buf[8][21];

volatile unsigned long current_frame = 0;

volatile bool flg_vsync = false;

static uint8_t x_cursor = 0;
static uint8_t y_cursor = 0;
static uint8_t count_candel = 0; // print_c_autoでdelが来た際に文字が何個消去できるかのカウント

uint8_t c_code_prev = '0';

// SPI1(LCD)の排他。
// 画面転送はcore1から、コマンド送信(lcd_scroll等)はBASICを実行するcore0から
// 呼ばれるため、CSとRSの操作が2コアで交錯しないように守る
auto_init_mutex(lcd_spi_mutex);

// 次に画面を転送する時刻[us]。lcd_task()が参照する
static uint32_t lcd_next_frame = 0;

void lcd_init(){

    // リセット
    gpio_put(PIN_LCD_RST, 0);
    sleep_ms(60);
    gpio_put(PIN_LCD_RST, 1);
    sleep_ms(60);

    lcd_write_command(0b10101110); // Display = OFF
    lcd_write_command(0b10100000); // ADC = normal
    lcd_write_command(0b11001000); // Common output = revers
    lcd_write_command(0b10100011); // bias = 1/7

    // 内部レギュレータを順にオン
    lcd_write_command(0b00101100); // power control 1
    sleep_ms(2);
    lcd_write_command(0b00101110); // power control 2
    sleep_ms(2);
    lcd_write_command(0b00101111); // poert control 3

    // コントラスト設定
    lcd_write_command(0b00100011); // Vo voltage resistor ratio set
    lcd_write_command(0b10000001); // Electronic volume mode set
    lcd_write_command(0b00011100); // Electronic volume value set

    // 表示設定
    lcd_write_command(0b10100100); // display all point = normal(全点灯しない)
    lcd_write_command(0b01000000); // display start line = 0
    lcd_write_command(0b10100110); // Display normal/revers = normal(白黒反転しない)
    lcd_write_command(0b10101111); // Display = ON

    // 画面のクリア
    lcd_cls(white, text);
    lcd_cls(white, graphic);

    lcd_next_frame = time_us_32();

}

void lcd_write_command(uint8_t cmd){

    mutex_enter_blocking(&lcd_spi_mutex);
    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_RS, 0);
    uint8_t src[1];
    src[0] = cmd;
    spi_write_blocking(spi1, src, 1);
    gpio_put(PIN_LCD_CS, 1);
    mutex_exit(&lcd_spi_mutex);

}

void lcd_write_data(uint8_t data){

    mutex_enter_blocking(&lcd_spi_mutex);
    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_RS, 1);
    uint8_t src[1];
    src[0] = data;
    spi_write_blocking(spi1, src, 1);
    gpio_put(PIN_LCD_CS, 1);
    mutex_exit(&lcd_spi_mutex);

}

void lcd_set_cursor(uint8_t x_sec, uint8_t y_sec){
    x_cursor = x_sec;
    y_cursor = y_sec;
}

void lcd_cls(color cl, screen sc){

    for(int i=0; i<8; i++){
        for(int j=0; j<128; j++){
            if(cl){
                if(sc == text) v_buf[i][j] = 0b11111111;
                else if(sc == graphic) vg_buf[i][j] = 0b11111111;
            } else {
                if(sc == text) v_buf[i][j] = 0b00000000;
                else if(sc == graphic) vg_buf[i][j] = 0b00000000;
            }
        }
    }

    if(sc == text){
      lcd_set_cursor(0, 0);
      for(int i=0; i<8; i++) {
        for(int j=0; j<21; j++) {
          scr_buf[i][j] = 0;
        }
      }
    }

}

void lcd_disp_vbuf(){

    mutex_enter_blocking(&lcd_spi_mutex);

    gpio_put(PIN_LCD_CS, 0);

    // 1バイトずつspi_write_blocking()を呼ぶと1フレームで1027回の
    // 呼び出しになり、60Hzのタイマ割り込みからこれを回すとBASICの
    // 実行時間をかなり食う。ページ単位でまとめて転送する。
    // この関数はcore1のlcd_task()からのみ呼ばれるので
    // 転送バッファはstaticで良い
    static uint8_t src[128];

    for(int i=0; i<8; i++){

        gpio_put(PIN_LCD_RS, 0); // コマンドの送信
        src[0] = 0b10110000+i; // ページアドレスの設定
        src[1] = 0b00010000; // コラムアドレスの設定(上位桁)
        src[2] = 0b00000000; // コラムアドレスの設定(下位桁)
        spi_write_blocking(spi1, src, 3);

        for(int j=0; j<128; j++){
            src[j] = v_buf[i][j] | vg_buf[i][j];
        }

        gpio_put(PIN_LCD_RS, 1); // ディスプレイデータの送信
        spi_write_blocking(spi1, src, 128);

    }

    gpio_put(PIN_LCD_CS, 1);

    mutex_exit(&lcd_spi_mutex);

    flg_vsync = true;

}

// vbufを1ライン上下にスライドする
void lcd_slide_vbuf(scroll_dir dir, color cl){

    if(dir){

        // 下の行にコピー
        for(int i=1; i<8; i++){
            for(int j=0; j<128; j++){
                v_buf[i][j] = v_buf[i-1][j];
            }
        }

        // 一番上の行を初期化
        for(int i=0; i<128; i++){
            if(cl) v_buf[0][i] = 0b11111111;
            else v_buf[0][i] = 0b00000000;
        }

        // 下の行にコピー
        for(int i=1; i<8; i++) {
          for(int j=0; j<21; j++) {
            scr_buf[i][j] = scr_buf[i-1][j];
          }
        }

        // 一番上の行を初期化
        for(int i=0; i<21; i++){
          scr_buf[0][i] = 0;
        }

    } else {

        // 上の行にコピー
        for(int i=0; i<7; i++){
            for(int j=0; j<128; j++){
                v_buf[i][j] = v_buf[i+1][j];
            }
        }

        // 一番下の行を初期化
        for(int i=0; i<128; i++){
            if(cl) v_buf[7][i] = 0b11111111;
            else v_buf[7][i] = 0b00000000;
        }

        // 上の行にコピー
        for(int i=0; i<7; i++){
          for(int j=0; j<21; j++){
            scr_buf[i][j] = scr_buf[i+1][j];
          }
        }

        // 一番下の行を初期化
        for(int i=0; i<21; i++){
          scr_buf[7][i] = 0;
        }
    }
}

// x_pos(0~127), y_pos(0~47), cl(white, black)
void lcd_pset(int16_t x_pos, int16_t y_pos, color cl, screen sc){

    if(x_pos < 0 || 127 < x_pos || y_pos < 0 || 63 < y_pos) return; // 画面外のドットは打たない

    uint8_t page, dot_extract;

    page = y_pos >> 3; // 8で割って表示するページ数を求める

    // 8で割った余りがそのままビット位置になる。
    // ここは以前1ビットずつ左シフトするループだったが、
    // lcd_line()やlcd_circle()からドット1個ごとに呼ばれるため、
    // 最大7回の空回りがそのまま描画時間に乗っていた
    dot_extract = (uint8_t)(1u << (y_pos & 0b00000111));

    // ドットの色を指定し、バッファと論理演算する
    if(cl){
        if(sc == text) v_buf[page][x_pos] |= dot_extract;
        else if(sc == graphic) vg_buf[page][x_pos] |= dot_extract;
    } else {
        dot_extract = ~dot_extract;
        if(sc == text) v_buf[page][x_pos] &= dot_extract;
        else if(sc == graphic) vg_buf[page][x_pos] &= dot_extract;

    }

}

// 水平線をバッファへ直接書く。
// lcd_pset()を1ドットずつ呼ぶと、横128ドットの線で128回の関数呼び出しと
// 128回のページ・ビット位置の再計算になる。同じ水平線上のドットは
// 全て同じページの同じビットなので、マスクを1度だけ作ってx方向に流せばよい。
// GRECTの塗りつぶしとGCIRCLEの塗りつぶしはこれを縦に繰り返すだけなので、
// ここが効くと図形描画全体が効く
static void lcd_hspan(int16_t x_pos0, int16_t x_pos1, int16_t y_pos, color cl, screen sc){

    if(y_pos < 0 || 63 < y_pos) return;

    if(x_pos0 > x_pos1){ int16_t t = x_pos0; x_pos0 = x_pos1; x_pos1 = t; }
    if(x_pos1 < 0 || 127 < x_pos0) return; // 完全に画面外
    if(x_pos0 < 0) x_pos0 = 0;
    if(x_pos1 > 127) x_pos1 = 127;

    uint8_t page = y_pos >> 3;
    uint8_t mask = (uint8_t)(1u << (y_pos & 0b00000111));
    volatile uint8_t *row = (sc == text) ? v_buf[page] : vg_buf[page];

    if(cl){
        for(int16_t x=x_pos0; x<=x_pos1; x++) row[x] |= mask;
    } else {
        uint8_t nmask = (uint8_t)~mask;
        for(int16_t x=x_pos0; x<=x_pos1; x++) row[x] &= nmask;
    }

}

// 垂直線をバッファへ直接書く。
// 縦は8ドットで1バイトに収まるので、ページごとにまとめてマスクを作れば
// 最大64回のlcd_pset()が最大8回のバイト演算になる
static void lcd_vspan(int16_t x_pos, int16_t y_pos0, int16_t y_pos1, color cl, screen sc){

    if(x_pos < 0 || 127 < x_pos) return;

    if(y_pos0 > y_pos1){ int16_t t = y_pos0; y_pos0 = y_pos1; y_pos1 = t; }
    if(y_pos1 < 0 || 63 < y_pos0) return; // 完全に画面外
    if(y_pos0 < 0) y_pos0 = 0;
    if(y_pos1 > 63) y_pos1 = 63;

    for(int16_t y=y_pos0; y<=y_pos1; ){

        uint8_t page = y >> 3;
        int16_t page_end = (page << 3) + 7; // このページが担当する最後のy
        if(page_end > y_pos1) page_end = y_pos1;

        // このページ内で塗る範囲のビットだけを立てる
        uint8_t mask = 0;
        for(int16_t yy=y; yy<=page_end; yy++) mask |= (uint8_t)(1u << (yy & 0b00000111));

        if(sc == text){
            if(cl) v_buf[page][x_pos] |= mask;
            else   v_buf[page][x_pos] &= (uint8_t)~mask;
        } else {
            if(cl) vg_buf[page][x_pos] |= mask;
            else   vg_buf[page][x_pos] &= (uint8_t)~mask;
        }

        y = page_end + 1;
    }

}

// ブレゼンハムのアルゴリズム
// x_pos0,x_pos1(0~127), y_pos0,y_pos1(0~47), cl(white,black)
void lcd_line(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, color cl){

    // 水平・垂直はブレゼンハムを通さず直接バッファを叩く。
    // 塗りつぶし系は全てこの2つに帰着するので、ここで分岐する価値がある
    if(y_pos0 == y_pos1){
        lcd_hspan(x_pos0, x_pos1, y_pos0, cl, graphic);
        return;
    }
    if(x_pos0 == x_pos1){
        lcd_vspan(x_pos0, y_pos0, y_pos1, cl, graphic);
        return;
    }


    int16_t tmp; // x,yの値入れ替え用
    int16_t delta_x, delta_y;
    int error; // 判定値
    int16_t x, y; // 点を打つ座標
    int16_t y_step; // 次の点が+1されるか-1されるか

    // アルゴリズム適用の前提条件
    // 傾きが45度以内になるように差分が大きい方を求め、必要ならx,yを入れ替える
    int steep = (abs((int)y_pos1-(int)y_pos0) > abs((int)x_pos1-(int)x_pos0));
    if(steep){
        tmp = x_pos0; x_pos0 = y_pos0; y_pos0 = tmp;
        tmp = x_pos1; x_pos1 = y_pos1; y_pos1 = tmp;
    }
    // x_pos1の方がx_pos0よりも大きくなるように入れ替える
    if(x_pos0 > x_pos1){
        tmp = x_pos0; x_pos0 = x_pos1; x_pos1 = tmp;
        tmp = y_pos0; y_pos0 = y_pos1; y_pos1 = tmp;
    }

    delta_x = x_pos1 - x_pos0;
    delta_y = abs((int)y_pos1 - (int)y_pos0);
    error = 0;
    y = y_pos0;

    // 傾きでステップの正負を切り替え
    if(y_pos0 < y_pos1){
        y_step = 1;
    } else {
        y_step = -1;
    }

    for(x=x_pos0; x<=x_pos1; x++){ // 次の点が直線の上にあるのか下にあるのか判定して点を打つ
        if(steep){
            lcd_pset(y, x, cl, graphic);
        } else {
            lcd_pset(x, y, cl, graphic);
        }
        error += delta_y;
        if((error << 1) >= delta_x){ // この式が成り立てば、次の点はy_step分移動する
            y += y_step;
            error -= delta_x;
        }
    }

}

void lcd_rect(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, color cl, bool fill){

    if(fill){
        // 1行ずつ水平スパンで塗る。
        // 以前は下側のループカウンタがuint8_tだったため、
        // y_pos1が負のときに終了条件を満たさず暴走する余地があった
        int16_t y_top = (y_pos1 >= y_pos0) ? y_pos0 : y_pos1;
        int16_t y_bottom = (y_pos1 >= y_pos0) ? y_pos1 : y_pos0;
        for(int16_t i=y_top; i<=y_bottom; i++){
            lcd_hspan(x_pos0, x_pos1, i, cl, graphic);
        }
    } else {
        lcd_line(x_pos0, y_pos0, x_pos1, y_pos0, cl);
        lcd_line(x_pos0, y_pos0, x_pos0, y_pos1, cl);
        lcd_line(x_pos1, y_pos0, x_pos1, y_pos1, cl);
        lcd_line(x_pos0, y_pos1, x_pos1, y_pos1, cl);
    }
}

void lcd_triangle(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, int16_t x_pos2, int16_t y_pos2, color cl, bool fill) {
  if(fill) {

    // 全ての座標が同じ場合
    if (x_pos0 == x_pos1 && x_pos1 == x_pos2 && y_pos0 == y_pos1 && y_pos1 == y_pos2) {
      lcd_pset(x_pos0, y_pos0, cl, graphic); // 1ドットだけ描く
      return;
    }

    // y座標順にソート
    if (y_pos0 > y_pos1) { int16_t t; t=y_pos0; y_pos0=y_pos1; y_pos1=t; t=x_pos0; x_pos0=x_pos1; x_pos1=t; }
    if (y_pos0 > y_pos2) { int16_t t; t=y_pos0; y_pos0=y_pos2; y_pos2=t; t=x_pos0; x_pos0=x_pos2; x_pos2=t; }
    if (y_pos1 > y_pos2) { int16_t t; t=y_pos1; y_pos1=y_pos2; y_pos2=t; t=x_pos1; x_pos1=x_pos2; x_pos2=t; }

    // 面積が0(=線分)のとき
    int area2 = (x_pos1 - x_pos0)*(y_pos2 - y_pos0) - (y_pos1 - y_pos0)*(x_pos2 - x_pos0);
    if (area2 == 0) {
      // 面積ゼロなので退化三角形
      // → 線分か点として描画
      lcd_line(x_pos0, y_pos0, x_pos1, y_pos1, cl);
      lcd_line(x_pos1, y_pos1, x_pos2, y_pos2, cl);
      return;
    }


    // 長辺 (y_pos0 → y_pos2) の傾き
    float dx13 = (float)(x_pos2 - x_pos0) / (y_pos2 - y_pos0);

    // 各スキャンラインで辺のxを頂点から再計算する
    // (累積加算による1行分のずれ・平坦辺でのゼロ除算を避ける)
    for (int16_t y = y_pos0; y <= y_pos2; y++) {
      float sx = (float)x_pos0 + dx13 * (y - y_pos0); // 長辺側
      float ex;                                       // もう一方の辺側
      if (y < y_pos1) {
        // 上半分の辺 (y_pos0 → y_pos1)
        ex = (float)x_pos0 + (float)(x_pos1 - x_pos0) * (y - y_pos0) / (y_pos1 - y_pos0);
      } else if (y_pos2 != y_pos1) {
        // 下半分の辺 (y_pos1 → y_pos2)
        ex = (float)x_pos1 + (float)(x_pos2 - x_pos1) * (y - y_pos1) / (y_pos2 - y_pos1);
      } else {
        ex = (float)x_pos1;
      }
      lcd_line((int16_t)lroundf(sx), y, (int16_t)lroundf(ex), y, cl);
    }
  } else {
    lcd_line(x_pos0, y_pos0, x_pos1, y_pos1, cl);
    lcd_line(x_pos1, y_pos1, x_pos2, y_pos2, cl);
    lcd_line(x_pos2, y_pos2, x_pos0, y_pos0, cl);
  }
}

void lcd_circle(int16_t x_pos, int16_t y_pos, uint8_t rad, color cl, bool fill){

    int x = rad;
    int y = 0;
    int F = -2 * rad + 3;
    while(x >= y){
        lcd_pset(x_pos+x, y_pos+y, cl, graphic);
        lcd_pset(x_pos-x, y_pos+y, cl, graphic);
        lcd_pset(x_pos+x, y_pos-y, cl, graphic);
        lcd_pset(x_pos-x, y_pos-y, cl, graphic);
        lcd_pset(x_pos+y, y_pos+x, cl, graphic);
        lcd_pset(x_pos-y, y_pos+x, cl, graphic);
        lcd_pset(x_pos+y, y_pos-x, cl, graphic);
        lcd_pset(x_pos-y, y_pos-x, cl, graphic);
        if(fill){
            lcd_line(x_pos-x, y_pos+y, x_pos+x, y_pos+y, cl);
            lcd_line(x_pos-x, y_pos-y, x_pos+x, y_pos-y, cl);
            lcd_line(x_pos-y, y_pos+x, x_pos+y, y_pos+x, cl);
            lcd_line(x_pos-y, y_pos-x, x_pos+y, y_pos-x, cl);
        }
        if(F >= 0){
            x--;
            F -= 4 * x;
        }
        y++;
        F += 4 * y + 2;
    }

}

// フォント1列(縦8ドット)をバッファへ書き込む書き方
typedef enum {
    font_opaque, // パターンの0/1をそのまま書く(背景も塗る)
    font_set,    // パターンが1のドットだけ立てる(黒の透過)
    font_clear   // パターンが0のドットだけ落とす(白の透過)
} font_write_mode;

// フォント1列分(縦8ドット)を(x_pos, y_pos)から書き込む。
// 以前はここをlcd_pset()で1ドットずつ8回叩いていたので、
// 1文字あたり40回の関数呼び出しとページ計算が発生していた。
// 縦8ドットは高々2ページにしかまたがらないので、
// シフトしたパターンとマスクを作って最大2バイトの演算で済ませる
static void lcd_font_col(uint8_t x_pos, int16_t y_pos, uint8_t pat,
                         screen sc, font_write_mode mode){

    if(x_pos > 127) return;

    // 8ドットが2ページにまたがる分をシフト量で表す
    uint8_t shift = y_pos & 0b00000111;
    // 上位側にはみ出した分を拾うため16ビットで持つ
    uint16_t pat16 = (uint16_t)pat << shift;
    uint16_t msk16 = (uint16_t)0x00ff << shift;

    for(int part=0; part<2; part++){

        int16_t page = (y_pos >> 3) + part;
        if(page < 0 || page > 7) continue;

        uint8_t p = (uint8_t)(pat16 >> (part * 8));
        uint8_t m = (uint8_t)(msk16 >> (part * 8));
        if(m == 0) continue; // このページには1ドットもかからない

        volatile uint8_t *cell = (sc == text) ? &v_buf[page][x_pos]
                                              : &vg_buf[page][x_pos];

        switch(mode){
        case font_opaque:
            *cell = (uint8_t)((*cell & ~m) | (p & m));
            break;
        case font_set:
            *cell |= (uint8_t)(p & m);
            break;
        case font_clear:
            // パターンが0でマスク内のビットを落とす
            *cell &= (uint8_t)~((uint8_t)(~p) & m);
            break;
        }
    }

}

void lcd_print_c_free(uint8_t x_pos, uint8_t y_pos, uint8_t c_code, color cl){

    uint8_t font_code, font_data_col;

    font_code = c_code; // 文字コードをプログラム内のフォントコードに変換

    for(font_data_col=0; font_data_col<5; font_data_col++) { // フォントデータを左側から順に表示
        // 白色ならフォントを反転させる (以前のドット単位の反転と同じ結果)
        uint8_t pat = cl ? font[font_code][font_data_col]
                         : (uint8_t)~font[font_code][font_data_col];
        lcd_font_col(x_pos, (int16_t)y_pos, pat, text, font_opaque);
        x_pos++;
    }

}

void lcd_gprint_c_free(uint8_t x_pos, uint8_t y_pos, uint8_t c_code, color cl, bool transparent){

    uint8_t font_code, font_data_col;

    font_code = c_code; // 文字コードをプログラム内のフォントコードに変換

    // 透過のときは、黒なら立っているドットだけを立て、
    // 白なら落ちているドットだけを落とす (元のドット単位の条件と同じ)
    font_write_mode mode;
    if(!transparent)  mode = font_opaque;
    else if(cl)       mode = font_set;
    else              mode = font_clear;

    for(font_data_col=0; font_data_col<5; font_data_col++) { // フォントデータを左側から順に表示
        uint8_t pat = cl ? font[font_code][font_data_col]
                         : (uint8_t)~font[font_code][font_data_col];
        lcd_font_col(x_pos, (int16_t)y_pos, pat, graphic, mode);
        x_pos++;
    }

}

// LCDを21x8に区切り、そのスペースに文字を打つ。freeと比較して処理が早くて手軽
// x_sec(0~20), y_sec(0~7: 表示領域は5まで)
void lcd_print_c_section(uint8_t x_sec, uint8_t y_sec, uint8_t c_code, color cl){

    if(x_sec > 20 || y_sec > 7) return; // 画面外は描写しない

    uint8_t font_code, font_data_col, dot_pos, dot_extract;
    bool dot_cl;

    font_code = c_code; // 文字コードをプログラム内のフォントコードに変換

    for(font_data_col=0; font_data_col<5; font_data_col++) { // フォントデータを左側から順に表示
        v_buf[y_sec][2+x_sec*6+font_data_col] = (cl) ? font[font_code][font_data_col] : ~font[font_code][font_data_col]; // 左端2列、右端1列空ける
    }

    scr_buf[y_sec][x_sec] = c_code;

}

// ターミナルのような感じで自動でスクロールするように文字を表示する
void lcd_print_c_auto(uint8_t c_code, color cl){

    if(c_code == '\r'){

        x_cursor = 0;
        y_cursor++;
        count_candel = 0;

    } else if(c_code == '\n') {

        if(c_code_prev != '\r'){ // \r\nで2回改行しないように処理
            x_cursor = 0;
            y_cursor++;
            count_candel = 0;
        }

    } else if(c_code == 0x08) {

        if(count_candel > 0) {
            count_candel--;
            if(x_cursor == 0) {
                x_cursor = 20;
                y_cursor--;
            } else {
                x_cursor--;
            }
        }

    } else {

        if(y_cursor > 5){
            for(int i=0; i<y_cursor-5; i++) { // 2回以上改行されたとき回数分改行する
                if(cl) lcd_slide_vbuf(up, white);
                else lcd_slide_vbuf(up, black);
            }
            x_cursor = 0;
            y_cursor = 5;
        }
        lcd_print_c_section(x_cursor, y_cursor, c_code, cl);
        x_cursor++;
        if(x_cursor > 20) {
            x_cursor = 0;
            y_cursor++;
        }
        count_candel++;
    }

    c_code_prev = c_code;
}

void lcd_print_str_free(uint8_t x_pos, uint8_t y_pos, char* str, color cl){

    uint8_t str_len = strlen(str);

    for(int i=0; i<str_len; i++){
        lcd_print_c_free(x_pos, y_pos, *str, cl);
        x_pos += 6;
        str++;
    }

}

void lcd_print_str_section(uint8_t x_sec, uint8_t y_sec, char* str, color cl){

    uint8_t str_len = strlen(str);

    for(int i=0; i<str_len; i++){
        lcd_print_c_section(x_sec, y_sec, *str, cl);
        x_sec++;
        str++;
    }

}

void lcd_disp_bmp(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans){

    bool bmp_buf[48][128];
    struct sd_bmp_info bmp_info = {0, 0};
    sd_read_bmp(bmp_buf, &bmp_info, file_name);

    for(int i=0; i<bmp_info.image_height; i++){
        for(int j=0; j<bmp_info.image_width; j++){
            lcd_pset(x_pos+j, y_pos+i, bmp_buf[i][j], graphic);
        }
    }

}

void lcd_disp_nbi(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans){

    bool nbi_buf[48][128];
    struct sd_nbi_info nbi_info = {0, 0};
    sd_read_nbi(nbi_buf, &nbi_info, file_name);

    for(int i=0; i<nbi_info.image_height; i++){
        for(int j=0; j<nbi_info.image_width; j++){
            lcd_pset(x_pos+j, y_pos+i, nbi_buf[i][j], graphic);
        }
    }

}

int lcd_play_nbm(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans, unsigned int from, unsigned int to){

    sd_card_t *pSD;
    FRESULT fr;
    FIL fil;
    bool nbm_buf[48][128];
    struct sd_nbm_info nbm_info = {0, 0};
    int result;

    result = sd_open_nbm(&pSD, &fr, &fil, &nbm_info, file_name);
    switch(result) {
        case SD_ERR_SDMOUNT:
            return LCD_ERR_SDMOUNT;
        case SD_ERR_CHDRIVE:
            return LCD_ERR_CHDRIVE;
        case SD_ERR_FNFOUND:
            return LCD_ERR_FNFOUND;
    }

    unsigned long start_frame = lcd_get_current_frame();

    for(int i=(int)from; i<=(int)to; i++){

        //if((lcd_get_current_frame() - start_frame) > i){ // 送れている場合はスキップ
        //    continue;
        //} else {
        //    lcd_vsync();
        //}

        sd_disp_nbm(&fil, nbm_buf, &nbm_info, i);
        for(int j=0; j<nbm_info.image_height; j++){
            for(int k=0; k<nbm_info.image_width; k++){
                lcd_pset(x_pos+k, y_pos+j, nbm_buf[j][k], graphic);
            }
        }
        lcd_vsync();
    }

    sd_close_nbm(&pSD, &fr, &fil, &nbm_info, file_name);
    
    return LCD_ERR_OK;
}

void lcd_scroll(scroll_dir dir){

    if(dir){
        for(int i=63; i>=0; i--){
            lcd_write_command(0b01000000 + i);
            sleep_ms(50);
        }
    } else {
        for(int i=1; i<=63; i++){
            lcd_write_command(0b01000000 + i);
            sleep_ms(50);
        }
        lcd_write_command(0b01000000);
    }

}

void lcd_reverse_color(disp_status ds){

    if(ds){
        lcd_write_command(0b10100111);
            printf("reverse \n");
    } else {
        lcd_write_command(0b10100110);
            printf("normal \n");
    }

}

void lcd_vsync(){
    while(true){
        if(flg_vsync){
            flg_vsync = false;
            break;
        }
    }
}

short lcd_scr(uint8_t x_pos, uint8_t y_pos) {

  return scr_buf[y_pos][x_pos];

}

bool lcd_pget(int16_t x_pos, int16_t y_pos) {

  if(x_pos < 0 || 127 < x_pos || y_pos < 0 || 63 < y_pos) return 0; // 範囲外の指定は0を返す

  uint8_t page, dot_extract;

  page = y_pos >> 3; // 8で割って表示するページ数を求める

  // 8で割った余りがそのままビット位置になる (lcd_pset()と同じ)
  dot_extract = (uint8_t)(1u << (y_pos & 0b00000111));

  return (v_buf[page][x_pos]|vg_buf[page][x_pos])&dot_extract;

}

// 画面転送を1フレーム分進める。core1のメインループから頻繁に呼ぶこと。
//
// 以前はcore0のrepeating_timer (割り込み) からlcd_disp_vbuf()を呼んでいた。
// 8MHzのSPIで1027バイトを送るのに約1msかかり、spi_write_blocking()は
// その間ビジーウェイトするため、16.67msごとに約1msをBASICの実行から
// 割り込みで奪っていた (およそ6〜7%)。
// core1はtud_task()を回すだけでほぼ空いているので、そちらへ移す。
// これでcore0のBASICは画面転送で一切止まらない
void lcd_task(){

    if((int32_t)(time_us_32() - lcd_next_frame) < 0) return;

    // 転送が遅れても取り返そうとフレームを連続で送らないよう、
    // 現在時刻を基準に次のフレームを決める
    lcd_next_frame = time_us_32() + 16667;

    lcd_disp_vbuf();
    current_frame++;

}

unsigned long lcd_get_current_frame(){

    return current_frame;

}
