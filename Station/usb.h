#ifndef _USB_H_
#define _USB_H_

#include <stdbool.h>
#include <stdint.h>



#define USB_DESCR_TYP_DEVICE		1
#define USB_DESCR_TYP_CONFIG		2
#define USB_DESCR_TYP_STRING		3
#define USB_DESCR_TYP_IFACE			4
#define USB_DESCR_TYP_ENDPT			5
#define USB_DESCR_TYP_DEV_QUAL		6
#define USB_DESCR_TYP_OTH_SPD_CFG	7
#define USB_DESCR_TYP_INT_PWR		8
#define USB_DESCR_TYP_HID			0x21
#define USB_DESCR_TYP_HID_REPORT	0x22
#define USB_DESCR_TYP_HID_PHYS		0x23

enum UsbEvent {
	UsbEventReset,
	UsbEventConfigSelected,
};

typedef int32_t (*UsbCustomEp0req)(uint8_t *dataOutP, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength);
typedef void (*UsbEventNotifF)(enum UsbEvent);


//only one iface supported! #0. bite me!
void usbInit(UsbCustomEp0req ep0req, UsbEventNotifF notif, const uint8_t *configDescr, uint32_t configDescrLen);
bool usbIsInited(void);
int32_t usbEpRx(uint8_t epNo, uint8_t *dst, uint32_t maxLen);
void usbEpTx(uint8_t epNo, const uint8_t *src, uint32_t len);

#endif
