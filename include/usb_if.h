#ifndef USB_IF_H
#define USB_IF_H

extern void usb_if_poll(void);
extern void usb_if_init(void);

#define USB_IF_REQUEST_RESET_WRITEPTR 0x0000
#define USB_IF_REQUEST_PANEL_ONOFF 0x0001
#define USB_IF_REQUEST_PANEL_BRIGHTNESS 0x0002
#define USB_IF_REQUEST_MBI5029_MODE 0x0003


#endif
