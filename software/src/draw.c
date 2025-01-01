#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "draw.h"
#include "lcd.h"

void draw_place_dot_order(uint8_t row_start, uint8_t row_end, color cl){

    for(int i=row_start; i<=row_end; i++){
        for(int j=0; j<=127; j++){
            printf("x_pos: %d, y_pos: %d\n", j, i);
            sleep_ms(10);
            lcd_pset(j, i, cl, graphic);
        }
    }

}

void draw_place_dot_random(color cl){

    sleep_ms(1);
    lcd_pset(rand()%128, rand()%48, cl, graphic);

}

void draw_place_c_order(color cl){

    for(int i=0; i<=63; i++){
        for(int j=0; j<=127; j++){
            printf("x_pos: %d, y_pos: %d\n", j, i);
            sleep_ms(50);
            lcd_print_c_free(j, i, 0x21, cl);
        }
    }

}

void draw_test(){

    //draw_place_dot_random(black);
    //for(int i=0; i<=7; i++){
    //    for(int j=0; j<=20; j++){
    //        if(0x20+21*i+j<=0x7F){
    //            lcd_print_c_section(j, i, 0x20+21*i+j, black);
    //        }
    //    }
    //}
    //lcd_print_str_free(2, 0, "Hello World.", black);
    //lcd_print_str_section(0, 1, "I'm NERD BOY.", black);
    //lcd_line(0, 20, 127, 32, black);
    //lcd_circle(50, 20, 5, black, true);
    //for(int i=0; i<13414; i++){
    //    char filename[128];
    //    sprintf(filename, "resize_edit/badapple_%04d.nbi", i);
    //    lcd_disp_nbi(0, 0, filename, black, false);
    //    //sleep_ms(16);
    //}
    //sleep_ms(5000);
    //lcd_disp_nbi(0, 0, "1.nbi", black, false);
    lcd_play_nbm(0, 0, "badapple3.nbm", black, false, 0, 13113);
    //while(1){
    //    sleep_ms(100);
    //    lcd_print_c_auto('1', black);
    //    sleep_ms(100);
    //    lcd_print_c_auto('2', black);
    //    sleep_ms(100);
    //    lcd_print_c_auto('3', black);
    //    sleep_ms(100);
    //    lcd_print_c_auto('\n', black);
    //}
    while(true);
    //lcd_scroll(down);
    //lcd_cls(white);
    //draw_place_dot_order(40, 43);
    //lcd_reverse_color(reverse);
    //sleep_ms(2000);
    //lcd_reverse_color(normal);
    //sleep_ms(2000);
    //lcd_cls(white);
    //lcd_scroll();

}
