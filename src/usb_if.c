#include "usb_if.h"
#include "ledpanel_buffer.h"

#include <stdlib.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/st_usbfs.h>

static usbd_device *usb_if_usbdev;
uint8_t usb_if_ctrl_buf[128];
uint8_t usb_if_rxbuf[64];

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_VENDOR,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor data_endp[] = { {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
} };

static const struct usb_interface_descriptor usb_ifdescr_ledmatrix[] = {
	{ .bLength = USB_DT_INTERFACE_SIZE,
	  .bDescriptorType = USB_DT_INTERFACE,
	  .bInterfaceNumber = 0,
	  .bAlternateSetting = 0,
	  .bNumEndpoints = 1,
	  .bInterfaceClass = USB_CLASS_VENDOR,
	  .bInterfaceSubClass = 0,
	  .bInterfaceProtocol = 0,
	  .iInterface = 0,
	  .endpoint = data_endp,
	  .extra = NULL,
	  .extralen = 0 }
};

static const struct usb_interface ifaces[] = { {
	.num_altsetting = 1,
	.altsetting = usb_ifdescr_ledmatrix,
} };

static const struct usb_config_descriptor usb_if_config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,
	.interface = ifaces,
};

static const char *usb_strings[] = {
	"github.com/vogelchr",
	"subway led sign",
	"0000001",
};

static uint8_t * const fb_start = (uint8_t*)ledpanel_buffer;
static uint8_t * const fb_end = ((void*)ledpanel_buffer) + sizeof(ledpanel_buffer);
uint8_t * fb_writep = (uint8_t*)ledpanel_buffer;

static void
usb_if_bulkout_cb(usbd_device *usbd_dev, uint8_t ep)
{
	unsigned short rx_len;
	const uint8_t *srcp, *src_end;

	(void)ep;

	rx_len = usbd_ep_read_packet(usbd_dev, 0x01, usb_if_rxbuf, sizeof(usb_if_rxbuf));

	srcp = usb_if_rxbuf;
	src_end = srcp + rx_len;

	while (srcp != src_end) {
		*fb_writep++ = *srcp++;
		if (fb_writep == fb_end)
			fb_writep = fb_start;
	}
}

static enum usbd_request_return_codes
usb_if_control_cb(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)buf;
	(void)len;
	(void)complete;
	(void)usbd_dev;

	if (req->bmRequestType != USB_REQ_TYPE_VENDOR)
		return USBD_REQ_NOTSUPP; /* Only accept vendor request. */

	/* the only action currently defined is to reset the framebuffer pointer! */
	fb_writep = fb_start;

	return USBD_REQ_HANDLED;
}

static void usb_if_config_cb(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_VENDOR,
				USB_REQ_TYPE_TYPE,
				usb_if_control_cb);

	usbd_ep_setup(usbd_dev,
		0x01, /* ep addr */
		USB_ENDPOINT_ATTR_BULK,
		64, /* max size */
		usb_if_bulkout_cb);
}

void usb_if_poll()
{
	/* this is a desperate attempt to fix the problem wher the
	   endpoint ends up in a NAK state, which I am too dumb to find
	   out why it's happening. */
#if 0
	if ((*USB_EP_REG(0x01) & USB_EP_RX_STAT) == USB_EP_RX_STAT_NAK)
		 USB_SET_EP_RX_STAT(0x01, USB_EP_RX_STAT_VALID);
#endif
	usbd_poll(usb_if_usbdev);
}

void usb_if_init()
{
	/* === USB === */
	usb_if_usbdev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &usb_if_config,
				  usb_strings, 3, usb_if_ctrl_buf,
				  sizeof(usb_if_ctrl_buf));
	usbd_register_set_config_callback(usb_if_usbdev, usb_if_config_cb);
}
