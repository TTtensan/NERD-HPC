#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "lcd.h"
#include "pin.h"
#include "hardware/spi.h"
#include "font.h"
#include "sd.hpp"

// 8ビットの塊が縦方向に6個、横方向に128個あるイメージ、上のドットは下位ビット
volatile uint8_t v_buf[8][128];
// グラフィック画面用バッファ
volatile uint8_t vg_buf[8][128];

volatile unsigned long current_frame = 0;

volatile bool flg_vsync = false;

static uint8_t x_cursor = 0;
static uint8_t y_cursor = 0;
static uint8_t count_candel = 0; // print_c_autoでdelが来た際に文字が何個消去できるかのカウント

uint8_t c_code_prev = '0';

void lcd_init(){

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

    lcd_start_disp_vbuf_timer();

}

void lcd_write_command(uint8_t cmd){

    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_RS, 0);
    uint8_t src[1];
    src[0] = cmd;
    spi_write_blocking(spi1, src, 1);
    gpio_put(PIN_LCD_CS, 1);

}

void lcd_write_data(uint8_t data){

    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_RS, 1);
    uint8_t src[1];
    src[0] = data;
    spi_write_blocking(spi1, src, 1);
    gpio_put(PIN_LCD_CS, 1);

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

}

void lcd_disp_vbuf(){

    gpio_put(PIN_LCD_CS, 0);

    uint8_t src[1];

    for(int i=0; i<8; i++){

        gpio_put(PIN_LCD_RS, 0); // コマンドの送信
        src[0] = 0b10110000+i; // ページアドレスの設定
        spi_write_blocking(spi1, src, 1);
        src[0] = 0b00010000; // コラムアドレスの設定(上位桁)
        spi_write_blocking(spi1, src, 1);
        src[0] = 0b00000000; // コラムアドレスの設定(下位桁)
        spi_write_blocking(spi1, src, 1);

        gpio_put(PIN_LCD_RS, 1); // ディスプレイデータの送信
        for(int j=0; j<128; j++){
            src[0] = v_buf[i][j] | vg_buf[i][j];
            spi_write_blocking(spi1, src, 1);
        }

    }

    gpio_put(PIN_LCD_CS, 1);

    flg_vsync = true;

}

// vbufを1ライン上下にスライドする
void lcd_slide_vbuf(scroll_dir dir, color cl){

    if(dir){

        // 下の行にコピー
        for(int i=1; i<8; i++){
            for(int j=1; j<128; j++){
                v_buf[i][j] = v_buf[i-1][j];
            }
        }

        // 一番上の行を初期化
        for(int i=0; i<128; i++){
            if(cl) v_buf[0][i] = 0b11111111;
            else v_buf[0][i] = 0b00000000;
        }

    } else {

        // 上の行にコピー
        for(int i=0; i<7; i++){
            for(int j=1; j<128; j++){
                v_buf[i][j] = v_buf[i+1][j];
            }
        }

        // 一番下の行を初期化
        for(int i=0; i<128; i++){
            if(cl) v_buf[7][i] = 0b11111111;
            else v_buf[7][i] = 0b00000000;
        }
    }
}

// x_pos(0~127), y_pos(0~47), cl(white, black)
void lcd_pset(uint8_t x_pos, uint8_t y_pos, color cl, screen sc){

    if(x_pos > 127 || y_pos > 63) return; // 画面外のドットは打たない

    uint8_t page, dot_extract;

    page = y_pos >> 3; // 8で割って表示するページ数を求める

    // 8で割った余りを求めて表示するビット位置を求める
    dot_extract = 0b00000001;
    for(uint8_t i=y_pos&0b00000111; i>0; i--){
        dot_extract <<= 1;
    }

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

// プレゼンハムのアルゴリズム
// x_pos0,x_pos1(0~127), y_pos0,y_pos1(0~47), cl(white,black)
void lcd_line(uint8_t x_pos0, uint8_t y_pos0, uint8_t x_pos1, uint8_t y_pos1, color cl){

    uint8_t tmp; // x,yの値入れ替え用
    uint8_t delta_x, delta_y;
    int error; // 判定値
    uint8_t x, y; // 点を打つ座標
    int8_t y_step; // 次の点が+1されるか-1されるか

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

void lcd_rect(uint8_t x_pos0, uint8_t y_pos0, uint8_t x_pos1, uint8_t y_pos1, color cl, bool fill){

    if(fill){
        if(y_pos1 >= y_pos0){
            for(uint8_t i=y_pos0; i<=y_pos1; i++){
                lcd_line(x_pos0, i, x_pos1, i, cl);
            }
        } else {
            for(uint8_t i=y_pos1; i<=y_pos0; i++){
                lcd_line(x_pos0, i, x_pos1, i, cl);
            }
        }
    } else {
        lcd_line(x_pos0, y_pos0, x_pos1, y_pos0, cl);
        lcd_line(x_pos0, y_pos0, x_pos0, y_pos1, cl);
        lcd_line(x_pos1, y_pos0, x_pos1, y_pos1, cl);
        lcd_line(x_pos0, y_pos1, x_pos1, y_pos1, cl);
    }
}

void lcd_circle(uint8_t x_pos, uint8_t y_pos, uint8_t rad, color cl, bool fill){

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

void lcd_print_c_free(uint8_t x_pos, uint8_t y_pos, uint8_t c_code, color cl){

    uint8_t font_code, font_data_col, dot_pos, dot_extract;
    bool dot_cl;

    font_code = c_code - 0x20; // 文字コードをプログラム内のフォントコードに変換

    for(font_data_col=0; font_data_col<5; font_data_col++) { // フォントデータを左側から順に表示
        dot_extract = 0b00000001;
        for(dot_pos=0; dot_pos<8; dot_pos++) { // フォントデータを列の上から1ビットずつ描画
            dot_cl = (font[font_code][font_data_col] & dot_extract) ? 1 : 0; // フォントデータ抽出
            if(!cl) dot_cl = !dot_cl; // 白色なら反転
            lcd_pset(x_pos, y_pos+dot_pos, dot_cl, text);
            dot_extract <<= 1; // フォントデータの抽出を次のビットに移行
        }
        x_pos++;
    }

}

// LCDを21x8に区切り、そのスペースに文字を打つ。freeと比較して処理が早くて手軽
// x_sec(0~20), y_sec(0~7: 表示領域は5まで)
void lcd_print_c_section(uint8_t x_sec, uint8_t y_sec, uint8_t c_code, color cl){

    if(x_sec > 20 || y_sec > 7) return; // 画面外は描写しない

    uint8_t font_code, font_data_col, dot_pos, dot_extract;
    bool dot_cl;

    font_code = c_code - 0x20; // 文字コードをプログラム内のフォントコードに変換

    for(font_data_col=0; font_data_col<5; font_data_col++) { // フォントデータを左側から順に表示
        v_buf[y_sec][2+x_sec*6+font_data_col] = (cl) ? font[font_code][font_data_col] : ~font[font_code][font_data_col]; // 左端2列、右端1列空ける
    }

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

bool repeating_timer_callback(struct repeating_timer *t) {

    lcd_disp_vbuf();
    current_frame++;
    return true;

}

void lcd_start_disp_vbuf_timer(){

    static struct repeating_timer timer;
    add_repeating_timer_us(-16667, repeating_timer_callback, NULL, &timer);

}

unsigned long lcd_get_current_frame(){

    return current_frame;

}
