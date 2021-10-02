#include "usb_tty.h"
#include "ledpanel_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

static usbd_device *usb_tty_usbdev;

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
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

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = { {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
} };

static const struct usb_endpoint_descriptor data_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x01,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1,
	},
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x82,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1,
	}
};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
    .header = {
	.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
	.bDescriptorType = CS_INTERFACE,
	.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
	.bcdCDC = 0x0110,
    },
    .call_mgmt = {
	.bFunctionLength = sizeof(struct usb_cdc_call_management_descriptor),
	.bDescriptorType = CS_INTERFACE,
	.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
	.bmCapabilities = 0,
	.bDataInterface = 1,
    },
    .acm = {
	.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
	.bDescriptorType = CS_INTERFACE,
	.bDescriptorSubtype = USB_CDC_TYPE_ACM,
	.bmCapabilities = 0,
    },
    .cdc_union = {
	.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
	.bDescriptorType = CS_INTERFACE,
	.bDescriptorSubtype = USB_CDC_TYPE_UNION,
	.bControlInterface = 0,
	.bSubordinateInterface0 = 1,
    }};

static const struct usb_interface_descriptor comm_iface[] = {
	{ .bLength = USB_DT_INTERFACE_SIZE,
	  .bDescriptorType = USB_DT_INTERFACE,
	  .bInterfaceNumber = 0,
	  .bAlternateSetting = 0,
	  .bNumEndpoints = 1,
	  .bInterfaceClass = USB_CLASS_CDC,
	  .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	  .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	  .iInterface = 0,
	  .endpoint = comm_endp,
	  .extra = &cdcacm_functional_descriptors,
	  .extralen = sizeof(cdcacm_functional_descriptors) }
};

static const struct usb_interface_descriptor data_iface[] = { {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
	.endpoint = data_endp,
} };

static const struct usb_interface ifaces[] = { {
						       .num_altsetting = 1,
						       .altsetting = comm_iface,
					       },
					       {
						       .num_altsetting = 1,
						       .altsetting = data_iface,
					       } };

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,
	.interface = ifaces,
};

static const char *usb_strings[] = {
	"github.com/vogelchr",
	"cbm8032sk keyboard emulator",
	"0000001",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes
cdcacm_control_request(struct _usbd_device *usbd_dev,
		       struct usb_setup_data *req,
		       /*uint8_t*/ unsigned char **buf,
		       /*uint16_t*/ short unsigned int *len,
		       void (**complete)(struct _usbd_device *usbd_dev,
					 struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
     * This Linux cdc_acm driver requires this to be implemented
     * even though it's optional in the CDC spec, and we don't
     * advertise it in the ACM functional descriptor.
     */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return USBD_REQ_HANDLED;
	}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding)) {
			return USBD_REQ_NOTSUPP;
		}
		return USBD_REQ_HANDLED;
	}
	return 0;
}

static char usb_rxbuf[64];
static char usb_txbuf[64];

/* quick hack to interact with the display using the simulated tty */

static unsigned int row_or_col = 0;
static unsigned int rowmode = 0;

static unsigned int x = 0;
static unsigned int y = 0;

static void put_nibble_at_xy(unsigned char nbl)
{
	unsigned int i;
	for (i = 0; i < 4; i++) {
		if (nbl & 1)
			LEDPANEL_SET(x, y);
		else
			LEDPANEL_CLR(x, y);
		x++;
		if (x >= LEDPANEL_PIX_WIDTH) {
			x = 0;
			y++;
		}
		if (y >= LEDPANEL_PIX_HEIGHT)
			y = 0;
		nbl >>= 1;
	}
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	int len, i, tx_len = 0;

	len = usbd_ep_read_packet(usbd_dev, 0x01, usb_rxbuf, sizeof(usb_rxbuf));

	for (i = 0; i < len; i++) {
		char c = usb_rxbuf[i];

		switch (c) {
		case 'z':
			x = 0;
			y = 0;
			break;
		case '0' ... '9':
			put_nibble_at_xy(c - '0');
			break;
		case 'A' ... 'F':
			put_nibble_at_xy(c - 'A' + 10);
			break;
		case 'r':
			tx_len = snprintf(usb_txbuf, sizeof(usb_txbuf),
					  "row mode selected\r\n");
			row_or_col = 0;
			rowmode = 1;
			break;
		case 'c':
			tx_len = snprintf(usb_txbuf, sizeof(usb_txbuf),
					  "column mode selected\r\n");
			row_or_col = 0;
			rowmode = 0;
			break;
		case '+':
		case '-':
			if (c == '+')
				row_or_col++;
			else
				row_or_col--;
			tx_len = snprintf(usb_txbuf, sizeof(usb_txbuf),
					  "%u\r\n", row_or_col);
			ledpanel_buffer_update(rowmode, row_or_col);
			break;
		}
	}

	if (tx_len)
		usbd_ep_write_packet(usbd_dev, 0x82, usb_txbuf, tx_len);
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64,
		      cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
		usbd_dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		cdcacm_control_request);
}

void usb_tty_poll()
{
	usbd_poll(usb_tty_usbdev);
}

void usb_tty_init()
{
	/* === USB === */
	usb_tty_usbdev =
		usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings,
			  3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usb_tty_usbdev, cdcacm_set_config);
}