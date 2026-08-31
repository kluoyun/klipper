#ifndef __ESP32_USB_CDC_EP_H
#define __ESP32_USB_CDC_EP_H

enum {
    USB_CDC_EP_BULK_IN = 1,
    USB_CDC_EP_BULK_OUT = 2,
    USB_CDC_EP_ACM = 3,
};

enum {
    USB_CDC_EP0_SIZE = 64,
    USB_CDC_EP_ACM_SIZE = 8,
    USB_CDC_EP_BULK_OUT_SIZE = 64,
    USB_CDC_EP_BULK_IN_SIZE = 64,
};

// End the largest transfer with a short packet.
#define USB_CDC_TRANSMIT_BUFFER_SIZE 255
#define USB_CDC_EP_BULK_IN_MAX_XFER 255

#endif // esp32/usb_cdc_ep.h
