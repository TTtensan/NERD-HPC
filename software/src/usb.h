#ifndef USB_H_
#define USB_H_

#ifdef __cplusplus
extern "C" {
#endif

void usb_set_keycode(uint8_t *code);
void usb_send_keycode_task(void);
void hid_task(void);

#ifdef __cplusplus
}
#endif

#endif
