#ifndef LCD_H_
#define LCD_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LCD_ERR_OK,
    LCD_ERR_SDMOUNT,
    LCD_ERR_CHDRIVE,
    LCD_ERR_FNFOUND
};

typedef enum {
    white,
    black
} color;

typedef enum {
    normal,
    reverse
} disp_status;

typedef enum {
    up,
    down
} scroll_dir;

typedef enum {
    text,
    graphic
} screen;

void lcd_init();
void lcd_write_command(uint8_t cmd);
void lcd_write_data(uint8_t data);
void lcd_set_cursor(uint8_t x_sec, uint8_t y_sec);
void lcd_cls(color cl, screen sc);
void lcd_disp_vbuf();
void lcd_slide_vbuf(scroll_dir dir, color cl);
void lcd_pset(int16_t x_pos, int16_t y_pos, color cl, screen sc);
void lcd_line(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, color cl);
void lcd_rect(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, color cl, bool fill);
void lcd_triangle(int16_t x_pos0, int16_t y_pos0, int16_t x_pos1, int16_t y_pos1, int16_t x_pos2, int16_t y_pos2, color cl, bool fill);
void lcd_circle(int16_t x_pos, int16_t y_pos, uint8_t rad, color cl, bool fill);
void lcd_print_c_free(uint8_t x_pos, uint8_t y_pos, uint8_t c_code, color cl);
void lcd_gprint_c_free(uint8_t x_pos, uint8_t y_pos, uint8_t c_code, color cl, bool transparent);
void lcd_print_c_section(uint8_t x_sec, uint8_t y_sec, uint8_t c_code, color cl);
void lcd_print_c_auto(uint8_t c_code, color cl);
void lcd_print_str_free(uint8_t x_pos, uint8_t y_pos, char* str, color cl);
void lcd_print_str_section(uint8_t x_sec, uint8_t y_sec, char* str, color cl);
void lcd_disp_bmp(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans);
void lcd_disp_nbi(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans);
int lcd_play_nbm(uint8_t x_pos, uint8_t y_pos, char* file_name, color cl, bool trans, unsigned int from, unsigned int to);
void lcd_scroll(scroll_dir dir);
void lcd_reverse_color(disp_status ds);
void lcd_vsync();
short lcd_scr(uint8_t x_pos, uint8_t y_pos);
bool lcd_pget(int16_t x_pos, int16_t y_pos);

bool repeating_timer_callback(struct repeating_timer *t);
void lcd_start_disp_vbuf_timer();
unsigned long lcd_get_current_frame();

#define abs(a) (((a)>0) ? (a) : -(a))

#ifdef __cplusplus
}
#endif

#endif
