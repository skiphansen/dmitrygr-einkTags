#include "nrf52840_bitfields.h"
#include "timebase.h"
#include "nrf52840.h"
#include "printf.h"
#include <string.h>
#include "usb.h"


///XXX: TODO seral no

#define GET_STATUS				0
#define CLEAR_FEATURE			1
#define SET_FEATURE				3
#define SET_ADDRESS				5
#define GET_DESCRIPTOR			6
#define SET_DESCRIPTOR			7
#define GET_CONFIGURATION		8
#define SET_CONFIGURATION		9
#define GET_INTERFACE			10
#define SET_INTERFACE			11
#define SYNCH_FRAME				12


#define NUM_CONFIGS				1
#define NUM_INTERFACES			1
#define USB_EP0_SIZE			64
#define USB_EP_SIZE				64
#define MILLIAMPS				200
#define VID						0xabcd
#define PID						0x1234

#define STRING_ID_LANGUAGE		0
#define STRING_ID_MANUF			1
#define STRING_ID_PRODUCT		2
#define STRING_ID_SERIAL		3
#define STRING_ID_CONFIG_NAME	4

#define STRING_ID_LANGUAGE		0
#define STRING_ID_MANUF			1
#define STRING_ID_PRODUCT		2
#define STRING_ID_SERIAL		3



static volatile bool mInited = false;
static uint8_t mCurConfig = 0;
static UsbCustomEp0req mEp0customReq;
static volatile uint8_t mRxBuf[8][USB_EP_SIZE];
static const uint8_t *mCofigDescr;
static uint32_t mConfigDescrLen;
static UsbEventNotifF mEvtNotifF;

struct UsbStringDescr {
	uint8_t descrLen;
	uint8_t descrTyp;
	uint16_t str[];
};

enum UsbConfigState {
	ConfigStateIsPossible,
	ConfigStateDisabled, 
	ConfigStateEnabled,
};

static const void* usbPrvGetDescriptor(uint32_t *descrSzP, uint8_t descrTyp, uint8_t descrIdx, uint16_t langIdx)
{
	static const struct UsbStringDescr strLanguage = {2 + 2 * 1, USB_DESCR_TYP_STRING, {0x0409,},};
	static const struct UsbStringDescr strManuf = {2 + 2 * 8, USB_DESCR_TYP_STRING, {'D', 'm', 'i', 't', 'r', 'y', 'G', 'R'},};
	static const struct UsbStringDescr strProduct = {2 + 2 * 14, USB_DESCR_TYP_STRING, {'e', 'I', 'n', 'k', 'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r'},};
	static const struct UsbStringDescr strSerial = {2 + 2 * 12, USB_DESCR_TYP_STRING, {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'},};
	static const uint8_t devDescr[] = {
		18,							// bLength
		USB_DESCR_TYP_DEVICE,			// bDescriptorType
		0x00, 0x02,					// bcdUSB (2.0)
		0,							// bDeviceClass
		0,							// bDeviceSubClass
		0,							// bDeviceProtocol
		USB_EP0_SIZE,				// bMaxPacketSize0
		(VID & 0xFF), (VID >> 8),	// idVendor
		(PID & 0xFF), (PID >> 8),	// idProduct
		0x00, 0x01,					// bcdDevice
		STRING_ID_MANUF,			// iManufacturer
		STRING_ID_PRODUCT,			// iProduct
		STRING_ID_SERIAL,			// iSerialNumber
		NUM_CONFIGS					// bNumConfigurations
	};
	
	switch (descrTyp) {
		case USB_DESCR_TYP_DEVICE:
			if (descrIdx == 0) {
				
				*descrSzP = sizeof(devDescr);
				return devDescr;
			}
			break;
		
		case USB_DESCR_TYP_CONFIG:
			if (descrIdx == 0) {	//0th config descr

				*descrSzP = mConfigDescrLen;
				return mCofigDescr;
			}
			break;
		
		case USB_DESCR_TYP_STRING:
			switch (descrIdx) {
				case STRING_ID_LANGUAGE:
					*descrSzP = strLanguage.descrLen;
					return (const uint8_t*)&strLanguage;
				
				case STRING_ID_MANUF:
					*descrSzP = strManuf.descrLen;
					return (const uint8_t*)&strManuf;
				
				case STRING_ID_PRODUCT:
					*descrSzP = strProduct.descrLen;
					return (const uint8_t*)&strProduct;
				
				case STRING_ID_SERIAL:
					*descrSzP = strSerial.descrLen;
					return (const uint8_t*)&strSerial;
				
				default:
					break;
			}
		
		default:
			pr("want descriptor type %d index %d lang 0x%04x\n", descrTyp, descrIdx, langIdx);
			break;
	}
	
	return NULL;
}

static bool usbPrvSetupEp0tx(const uint8_t *descr, uint32_t len)
{
	static const uint8_t *curDescr;
	static uint32_t remainLen;
	static uint64_t buf[64];
	
	if (descr) {
		curDescr = descr;
		remainLen = len;
	}
	
	if (remainLen) {
		
		uint32_t now = remainLen;
		
		if (now > USB_EP0_SIZE)
			now = USB_EP0_SIZE;
		
		memcpy(buf, curDescr, now);
		curDescr += now;
		remainLen -= now; 
		
		NRF_USBD->EPIN[0].PTR = (uintptr_t)buf;
		NRF_USBD->EPIN[0].MAXCNT = now;
		NRF_USBD->TASKS_STARTEPIN[0] = 1;
	}
	else if (curDescr) {	//we had a descr but no longer do
		
		curDescr = NULL;
		NRF_USBD->TASKS_EP0STATUS = 1;
	}
	else
		return false;
	
	return true;
}

static void usbPrvSendInBuffer(uint8_t epNo, uint32_t len)
{
	uint32_t mask = 1 << epNo;
	
	NRF_USBD->EPIN[epNo].PTR = (uintptr_t)mRxBuf[epNo];
	NRF_USBD->EPIN[epNo].MAXCNT = len;
	
	NRF_USBD->EPDATASTATUS = mask;
	NRF_USBD->TASKS_STARTEPIN[epNo] = 1;
	
	while (!(NRF_USBD->EPDATASTATUS & mask));
}

int32_t usbEpRx(uint8_t epNo, uint8_t *dst, uint32_t maxLen)
{
	uint32_t len, mask = 1 << (16 + epNo);
	uint32_t buf[(USB_EP_SIZE + sizeof(uint32_t) - 1) / sizeof(uint32_t)] = {};
	
	if (!(NRF_USBD->EPDATASTATUS & mask))
		return -1;
	
	NRF_USBD->EPDATASTATUS = mask;
	
	NRF_USBD->EVENTS_ENDEPOUT[epNo] = 0;
	NRF_USBD->EPOUT[epNo].PTR = (uintptr_t)buf;
	NRF_USBD->EPOUT[epNo].MAXCNT = USB_EP_SIZE;
	NRF_USBD->TASKS_STARTEPOUT[epNo] = 1;
	while (!(NRF_USBD->EVENTS_ENDEPOUT[epNo]));
	
	len = NRF_USBD->SIZE.EPOUT[epNo];
	if (len > maxLen)
		len = maxLen;
	
	asm volatile("":::"memory");
	memcpy(dst, buf, len);
	asm volatile("":::"memory");
	
	return len;
}

void usbEpTx(uint8_t epNo, const uint8_t *src, uint32_t len)
{
	uint32_t now;
	
	while (len) {
		
		now = len;
		if (now > USB_EP_SIZE)
			now = USB_EP_SIZE;
		
		asm volatile("":::"memory");
		memcpy((void*)mRxBuf[epNo], src, now);
		asm volatile("":::"memory");
		
		usbPrvSendInBuffer(epNo, now);
		
		src += now;
		len -= now;
	}
}

static bool usbPrvConfigSetState(enum UsbConfigState state, uint8_t cfgNo)
{
	if (cfgNo == 0)
		return true;
	
	if (cfgNo > NUM_CONFIGS)
		return false;
	
	if (cfgNo == 1) {
		
		switch (state) {
			case ConfigStateIsPossible:
				break;
			
			case ConfigStateDisabled:
				NRF_USBD->EPSTALL = 0x100 | 0x81;	//stall our EPs
				NRF_USBD->EPSTALL = 0x100 | 0x02;
				NRF_USBD->EPOUTEN &=~ (1 << 2);
				NRF_USBD->EPINEN &=~ (1 << 1);
				break;
				
			case ConfigStateEnabled:
				NRF_USBD->EPOUTEN |= (1 << 2);
				NRF_USBD->EPINEN |= (1 << 1);
				NRF_USBD->EPSTALL = 0x81;	//unstall our EPs
				NRF_USBD->EPSTALL = 0x02;
				mEvtNotifF(UsbEventConfigSelected);
				
				NRF_USBD->SIZE.EPOUT[2] = 0;
				
				pr("EPs configured\n");
				mInited = true;
				break;
			
			default:
				return false;
		}
		return true;
	}
	
	return false;
}

bool usbIsInited(void)
{
	return mInited;
}

void __attribute__((used)) USBD_IRQHandler(void)
{
	const void *descr = NULL;
	uint32_t len, ret;
	int32_t t;
	
	
	if (NRF_USBD->EVENTS_USBRESET) {
		pr("RESET\n");
		mEvtNotifF(UsbEventReset);
		mInited = false;
		NRF_USBD->EVENTS_USBRESET = 0;
	}
	
	if (NRF_USBD->EVENTS_EP0DATADONE) {
		
		NRF_USBD->EVENTS_EP0DATADONE = 0;
		
		if (!usbPrvSetupEp0tx(NULL, 0))
			NRF_USBD->TASKS_EP0STALL = 1;
	}
	if (NRF_USBD->EVENTS_EP0SETUP){
		
		uint16_t wLength = (((uint16_t)(uint8_t)NRF_USBD->WLENGTHH) << 8) + NRF_USBD->WLENGTHL;
		uint16_t wValue = (((uint16_t)(uint8_t)NRF_USBD->WVALUEH) << 8) + NRF_USBD->WVALUEL;
		uint16_t wIndex = (((uint16_t)(uint8_t)NRF_USBD->WINDEXH) << 8) + NRF_USBD->WINDEXL;
		bool handled = false;
		
		NRF_USBD->EVENTS_EP0SETUP = 0;
		
		switch ((NRF_USBD->BMREQUESTTYPE & USBD_BMREQUESTTYPE_TYPE_Msk) >> USBD_BMREQUESTTYPE_TYPE_Pos) {
			
			case USBD_BMREQUESTTYPE_TYPE_Standard:
				
				switch (NRF_USBD->BREQUEST) {
					
					case GET_DESCRIPTOR:		//get descriptor
						
						len = wLength;
						descr = usbPrvGetDescriptor(&ret, NRF_USBD->WVALUEH, NRF_USBD->WVALUEL, wIndex);
						if (descr) {
							if (ret > len)
								ret = len;
							handled = usbPrvSetupEp0tx(descr, ret);
						}
						break;
					
					case SET_ADDRESS:
						handled = true;
						break;
					
					case SET_CONFIGURATION:
						if (NRF_USBD->BMREQUESTTYPE)
							break;
						
						if (!usbPrvConfigSetState(ConfigStateIsPossible, NRF_USBD->WVALUEL))
							break;
						
						usbPrvConfigSetState(ConfigStateDisabled, mCurConfig);
						usbPrvConfigSetState(ConfigStateEnabled, NRF_USBD->WVALUEL);	//cannot fail, ConfigStateIsPossible shoudl have then...
						
						//ack
						NRF_USBD->TASKS_EP0STATUS = 1;
						handled = true;
						break;
					
					default:
						break;
				}
				break;
		}
		
		if (!handled && mEp0customReq) {
			
			static uint8_t smallBuf[64];
			
			t = mEp0customReq(smallBuf, NRF_USBD->BMREQUESTTYPE, NRF_USBD->BREQUEST, wValue, wIndex, wLength);
			handled = true;
			
			if (t > 0)
				usbPrvSetupEp0tx(smallBuf, t);
			else if (t == 0)
				NRF_USBD->TASKS_EP0STATUS = 1;
			else
				handled = false;
		}
		
		if (!handled)
			NRF_USBD->TASKS_EP0STALL = 1;
	}
	
	(void)NRF_USBD->EVENTS_USBRESET;
}

void usbInit(UsbCustomEp0req ep0req, UsbEventNotifF notif, const uint8_t *configDescr, uint32_t configDescrLen)
{
	uint64_t time;
	
	pr("usb init\n");
	
	mEp0customReq = ep0req;
	mEvtNotifF = notif;
	mCofigDescr = configDescr;
	mConfigDescrLen = configDescrLen;
	
	//peripheral on, diconnected
	
	*(volatile uint32_t *)0x4006EC00 = 0x00009375;
	*(volatile uint32_t *)0x4006ED14 = 0x00000003;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375;
	
	NRF_USBD->ENABLE = 1;
	
	/* Waiting for peripheral to enable, this should take a few us */
	while (0 == (NRF_USBD->EVENTCAUSE & USBD_EVENTCAUSE_READY_Msk))
	{
	    /* Empty loop */
	}
	NRF_USBD->EVENTCAUSE &= ~USBD_EVENTCAUSE_READY_Msk;
	
	*(volatile uint32_t *)0x4006EC00 = 0x00009375;
	*(volatile uint32_t *)0x4006ED14 = 0x00000000;
	*(volatile uint32_t *)0x4006EC00 = 0x00009375;
	
	
	pr("enabling ints\n");
	
	//ints cleared, disabled, NVIC handler on
	NRF_USBD->INTENCLR = 0xffffffff;
	(void)NRF_USBD->INTENCLR;			//read to force the write
	NVIC_SetPriority(USBD_IRQn, 4);
	NVIC_ClearPendingIRQ(USBD_IRQn);
	NVIC_EnableIRQ(USBD_IRQn);
	
	pr("setting allowed ints\n");
	NRF_USBD->INTENSET = USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_USBRESET_Msk | USBD_INTEN_EP0DATADONE_Msk;
	
	pr("pullup off\n");
	NRF_USBD->USBPULLUP = 0;
	time = timebaseGet();
	while (timebaseGet() - time < TIMER_TICKS_PER_SECOND / 3);
	pr("pullup on\n");
	NRF_USBD->USBPULLUP = 1;
}