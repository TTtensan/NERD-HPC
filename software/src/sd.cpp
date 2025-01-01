#include <stdlib.h>
#include <vector>
#include "sd.hpp"
#include "sd_card.h"
#include "hw_config.h"

#include <stdio.h>
#include <string.h>
#include "f_util.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "rtc.h"
#include "diskio.h"
#include "lcd.h"
#include "pin.h"

static std::vector<spi_t *> spis;
static std::vector<sd_card_t *> sd_cards;

size_t sd_get_num() { return sd_cards.size(); }
sd_card_t *sd_get_by_num(size_t num) {

    if (num <= sd_get_num()) {
        return sd_cards[num];
    } else {
        return NULL;
    }

}

size_t spi_get_num() { return spis.size(); }
spi_t *spi_get_by_num(size_t num) {

    if (num <= sd_get_num()) {
        return spis[num];
    } else {
        return NULL;
    }

}

void add_spi(spi_t *spi) { spis.push_back(spi); }
void add_sd_card(sd_card_t *sd_card) { sd_cards.push_back(sd_card); }

static spi_t *p_spi;

void sd_test(sd_card_t *pSD) {

    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    fr = f_chdrive(pSD->pcName);
    if (FR_OK != fr) panic("f_chdrive error: %s (%d)\n", FRESULT_str(fr), fr);

    FIL fil;
    const char *const filename = "filename.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        printf("f_printf failed\n");
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    
    f_unmount(pSD->pcName);

}

void sd_read_bmp(bool bmp_buf[48][128], struct sd_bmp_info* bmp_info, char* file_name){

    unsigned long previous_frame;
    unsigned long processed_frame;

    sd_card_t *pSD = sd_get_by_num(0);

    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    fr = f_chdrive(pSD->pcName);
    if (FR_OK != fr) panic("f_chdrive error: %s (%d)\n", FRESULT_str(fr), fr);

    FIL fil;
    fr = f_open(&fil, file_name, FA_OPEN_EXISTING | FA_READ);
    if (FR_OK != fr && FR_EXIST != fr){
        panic("f_open(%s) error: %s (%d)\n", file_name, FRESULT_str(fr), fr);
    }

    UINT read_buf_size = 4;
    uint8_t data[read_buf_size];
    UINT* read_byte_counter;

    // 画像幅取得
    f_lseek(&fil, 0x12);
    f_read(&fil, data, read_buf_size, read_byte_counter);
    bmp_info->image_width = data[3] << 3 | data[2] << 2 | data[1] << 1 | data[0];

    // 画像高さ取得
    f_lseek(&fil, 0x16);
    f_read(&fil, data, read_buf_size, read_byte_counter);
    bmp_info->image_height = data[3] << 3 | data[2] << 2 | data[1] << 1 | data[0];

    // 画像データオフセット位置取得
    f_lseek(&fil, 0x0A);
    f_read(&fil, data, read_buf_size, read_byte_counter);
    uint32_t image_offset = data[3] << 3 | data[2] << 2 | data[1] << 1 | data[0];

    previous_frame = lcd_get_current_frame();

    // 取得
    for(int i=0; i<bmp_info->image_height; i++){
        for(int j=0; j<bmp_info->image_width; j++){
            f_lseek(&fil, image_offset+((uint32_t)i*bmp_info->image_width+(uint32_t)j)*3);
            f_read(&fil, data, 3, read_byte_counter);
            bmp_buf[bmp_info->image_height-1-i][j] = ((data[2] == 0xFF && data[1] == 0xFF && data[0] == 0xFF)) ? 0 : 1; // 完全な白以外はドットを打つ
        }
    }

    processed_frame = lcd_get_current_frame();

    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    
    f_unmount(pSD->pcName);

    printf("process time: %lu\n", processed_frame-previous_frame);

}

void sd_read_nbi(bool nbi_buf[48][128], struct sd_nbi_info* nbi_info, char* file_name){

    unsigned long previous_frame;
    unsigned long processed_frame;

    sd_card_t *pSD = sd_get_by_num(0);

    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    fr = f_chdrive(pSD->pcName);
    if (FR_OK != fr) panic("f_chdrive error: %s (%d)\n", FRESULT_str(fr), fr);

    previous_frame = lcd_get_current_frame();
    FIL fil;
    fr = f_open(&fil, file_name, FA_OPEN_EXISTING | FA_READ);
    if (FR_OK != fr && FR_EXIST != fr){
        panic("f_open(%s) error: %s (%d)\n", file_name, FRESULT_str(fr), fr);
    }
    processed_frame = lcd_get_current_frame();

    UINT read_buf_size = 770;
    uint8_t data[read_buf_size];
    UINT* read_byte_counter;

    // 一気に読み込む
    f_read(&fil, data, read_buf_size, read_byte_counter);

    // 画像幅取得
    nbi_info->image_width = data[0];

    // 画像高さ取得
    nbi_info->image_height = data[1];

    // バッファにセット、ビットを順に詰めていくイメージ
    uint8_t current_bit = 7;
    uint16_t current_data = 2;
    for(int i=0; i<nbi_info->image_height; i++){
        for(int j=0; j<nbi_info->image_width; j++){
            uint8_t ext_bit = 1 << current_bit;
            nbi_buf[i][j] = data[current_data] & ext_bit;
            if(current_bit == 0){
                current_bit = 7;
                current_data++;
            } else {
                current_bit--;
            }
        }
    }

    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    
    f_unmount(pSD->pcName);

    printf("process time: %lu\n", processed_frame-previous_frame);

}

int sd_open_nbm(sd_card_t **pSD, FRESULT* fr, FIL* fil, struct sd_nbm_info* nbm_info, char* file_name){

    *pSD = sd_get_by_num(0);

    *fr = f_mount(&(*pSD)->fatfs, (*pSD)->pcName, 1);
    if (FR_OK != *fr) return SD_ERR_SDMOUNT;
    *fr = f_chdrive((*pSD)->pcName);
    if (FR_OK != *fr) {
        *fr = f_unmount((*pSD)->pcName);
        if (FR_OK == *fr) {
            (*pSD)->mounted = false;
        } else {
            printf("f_unmount error: %s (%d)\n", FRESULT_str(*fr), *fr);
        }
        (*pSD)->m_Status |= STA_NOINIT; // in case medium is removed
        sd_card_detect(*pSD);
        return SD_ERR_CHDRIVE;
    }

    *fr = f_open(fil, file_name, FA_OPEN_EXISTING | FA_READ);
    if (FR_OK != *fr && FR_EXIST != *fr) {
        *fr = f_unmount((*pSD)->pcName);
        if (FR_OK == *fr) {
            (*pSD)->mounted = false;
        } else {
            printf("f_unmount error: %s (%d)\n", FRESULT_str(*fr), *fr);
        }
        (*pSD)->m_Status |= STA_NOINIT; // in case medium is removed
        sd_card_detect(*pSD);
        return SD_ERR_FNFOUND;
    }

    UINT read_buf_size = 2;
    uint8_t data[read_buf_size];
    UINT* read_byte_counter;

    f_read(fil, data, read_buf_size, read_byte_counter);

    // 画像幅取得
    nbm_info->image_width = data[0];

    // 画像高さ取得
    nbm_info->image_height = data[1];

    return SD_ERR_OK;
}

void sd_close_nbm(sd_card_t **pSD, FRESULT* fr, FIL* fil, struct sd_nbm_info* nbm_info, char* file_name){

    *fr = f_close(fil);
    if (FR_OK != *fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(*fr), *fr);
    }
    
    *fr = f_unmount((*pSD)->pcName);
    if (FR_OK == *fr) {
        (*pSD)->mounted = false;
    } else {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(*fr), *fr);
    }
    (*pSD)->m_Status |= STA_NOINIT; // in case medium is removed
    sd_card_detect(*pSD);
}

void sd_disp_nbm(FIL* fil, bool nbm_buf[48][128], struct sd_nbm_info* nbm_info, unsigned int page){

    UINT read_buf_size = 768;
    uint8_t data[read_buf_size];
    UINT* read_byte_counter;

    // 該当ページの先頭に移動
    f_lseek(fil, 2+page*nbm_info->image_width*nbm_info->image_height/8);

    // 一気に読み込む
    f_read(fil, data, read_buf_size, read_byte_counter);

    // バッファにセット、ビットを順に詰めていくイメージ
    uint8_t current_bit = 7;
    uint16_t current_data = 0;
    for(int i=0; i<nbm_info->image_height; i++){
        for(int j=0; j<nbm_info->image_width; j++){
            uint8_t ext_bit = 1 << current_bit;
            nbm_buf[i][j] = data[current_data] & ext_bit;
            if(current_bit == 0){
                current_bit = 7;
                current_data++;
            } else {
                current_bit--;
            }
        }
    }

}

void sd_init(){

    time_init();

    // Hardware Configuration of SPI "object"
    p_spi = new spi_t;
    memset(p_spi, 0, sizeof(spi_t));
    if (!p_spi) panic("Out of memory");
    p_spi->hw_inst = spi0;  // SPI component
    p_spi->miso_gpio = PIN_SD_DAT0;  // GPIO number (not pin number)
    p_spi->mosi_gpio = PIN_SD_CMD;
    p_spi->sck_gpio = PIN_SD_CLK;
    p_spi->baud_rate = 4500 * 1000; 
    add_spi(p_spi);

    // Hardware Configuration of the SD Card "object"
    sd_card_t *p_sd_card = new sd_card_t;
    if (!p_sd_card) panic("Out of memory");
    memset(p_sd_card, 0, sizeof(sd_card_t));
    p_sd_card->pcName = "0:";  // Name used to mount device
    p_sd_card->spi = p_spi;    // Pointer to the SPI driving this card
    p_sd_card->ss_gpio = PIN_SD_DAT3;   // The SPI slave select GPIO for this SD card
    p_sd_card->use_card_detect = false;
    p_sd_card->card_detect_gpio = 13;  // Card detect
    // What the GPIO read returns when a card is
    // present. Use -1 if there is no card detect.
    p_sd_card->card_detected_true = -1;
    add_sd_card(p_sd_card);

}
