#ifndef SD_H_
#define SD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sd_card.h"

struct sd_bmp_info{
    uint32_t image_width;
    uint32_t image_height;
};

struct sd_nbi_info{
    uint32_t image_width;
    uint32_t image_height;
};

struct sd_nbm_info{
    uint32_t image_width;
    uint32_t image_height;
};

void sd_test(sd_card_t *pSD);
void sd_read_bmp(bool bmp_buf[48][128], struct sd_bmp_info* bmp_info, char* file_name);
void sd_read_nbi(bool nbi_buf[48][128], struct sd_nbi_info* nbi_info, char* file_name);
void sd_open_nbm(sd_card_t *pSD, FRESULT* fr, FIL* fil, struct sd_nbm_info* nbm_info, char* file_name);
void sd_close_nbm(sd_card_t *pSD, FRESULT* fr, FIL* fil, struct sd_nbm_info* nbm_info, char* file_name);
void sd_disp_nbm(FIL* fil, bool nbm_buf[48][128], struct sd_nbm_info* nbm_info, unsigned int page);
void sd_init();

#ifdef __cplusplus
}
#endif

#endif
