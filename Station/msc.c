#include <string.h>
#include "printf.h"
#include "msc.h"
#include "usb.h"

#define EP_NUM_IN		1
#define EP_NUM_OUT		2
#define EP_PACKET_SZ	64
#define MY_IFACE_IDX	0

static const uint8_t mConfigDescr[] = {
	9,							// bLength: length of descriptor in bytes
	USB_DESCR_TYP_CONFIG,		// bDescriptorType: descriptor type
	(9 + 9 + 7 + 7) & 0xFF,		// total length of data returned 
	(9 + 9 + 7 + 7) >>8,		//  (incl inlined descriptors)
	1,							// number of interfaces in this config
	1,							// index of this configuration
	0,							// configuration name string index (0 for none)
	(1 << 7),					// attributes
	MSC_DEVICE_CURRENT_MA / 2,	// max USB current in units of 2mA
	
	//interface descriptors follow
	9,							// bLength: length of descriptor in bytes
	USB_DESCR_TYP_IFACE,		// bDescriptorType: descriptor type
	MY_IFACE_IDX,				// bInterfaceNumber
	0,							// bAlternateSetting: 0
	2,							// bNumEndpoints: 2
	8,							// bInterfaceClass: MASS STORAGE
	6,							// bInterfaceSubClass: SCSI transparent command set
	0x50,						// bInterfaceProtocol: BULK-ONLY TRANSPORT
	0,							// iInterface - string index for string descriptod describing this interface: none
	
	//now the two EPs:
	7,							// bLength: length of descriptor in bytes
	USB_DESCR_TYP_ENDPT,		// bDescriptorType: descriptor type
	0x80 + EP_NUM_IN,			// bEndpointAddress: EP IN, #EP_NUM_IN
	2,							// bmAttributes: BULK EP
	EP_PACKET_SZ,				// wMaxPacketSize: low byte
	EP_PACKET_SZ >> 8,			// wMaxPacketSize: high byte
	0,							// bInterval: unused
	
	7,							// bLength: length of descriptor in bytes
	USB_DESCR_TYP_ENDPT,		// bDescriptorType: descriptor type
	EP_NUM_OUT,					// bEndpointAddress: EP OUT #EP_NUM_OUT
	2,							// bmAttributes: BULK EP
	EP_PACKET_SZ,				// wMaxPacketSize: low byte
	EP_PACKET_SZ >> 8,			// wMaxPacketSize: high byte
	0,							// bInterval: unused
};


#define SCSI_SENSE_NO_SENSE							0
#define SCSI_SENSE_RECOVERED_ERROR					1
#define SCSI_SENSE_NOT_READY						2
#define SCSI_SENSE_MEDIUM_ERROR						3
#define SCSI_SENSE_HARDWARE_ERROR					4
#define SCSI_SENSE_ILLEGAL_REQUEST					5

#define SCSI_ASC_NO_ADDITIONAL_SENSE_INFORMATION	0x00
#define SCSI_ASC_INVALID_FIELD_IN_CDB				0x24


enum MscState {
	MscStateInited,
	MscStateUsbReady,
};

struct Msc {
	uint32_t numBlocks, blockSz;
	MscDeviceRead readF;
	MscDeviceWrite writeF;
	void* cbkData;
	enum MscState state;
	uint8_t sense, senseAdditional;
};

static struct Msc mMsc;


static int32_t mscPrvUsbCustomEp0req(uint8_t *dataOutP, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
	if (bmRequestType == 0x21 && bRequest == 0xff && wValue == 0 && wIndex == MY_IFACE_IDX && wLength == 0) {
		
		pr("BULK STORAGE RESET\n");
		return 0;
	}
	
	if (bmRequestType == 0xa1 && bRequest == 0xfe && wValue == 0 && wIndex == MY_IFACE_IDX && wLength == 1) {
		
		pr("BULK STORAGE GET NUM LUNS\n");
		*dataOutP = 0;	//we have just one lun: #0
		return 1;
	}
	
	pr("request type 0x%02x val 0x%02x\n", bmRequestType, bRequest);
	
	return -1;
}

static void mscPrvUsbEventNotif(enum UsbEvent evt)
{
	
}

bool mscInit(uint32_t numSec, uint32_t secSize, MscDeviceRead readF, MscDeviceWrite writeF, void *userData)
{
	bzero(&mMsc, sizeof(struct Msc));
	mMsc.state = MscStateInited;
	mMsc.numBlocks = numSec;
	mMsc.blockSz = secSize;
	mMsc.readF = readF;
	mMsc.writeF = writeF;
	mMsc.cbkData = userData;
	
	usbInit(mscPrvUsbCustomEp0req, mscPrvUsbEventNotif, mConfigDescr, sizeof(mConfigDescr));
	
	return true;
}

static uint32_t mscPrvRead(uint32_t lba, uint32_t nSec)
{
	uint8_t buf[mMsc.blockSz];
	uint32_t numDone = 0;
	
	while (nSec--) {
		
		if (1 != mMsc.readF(mMsc.cbkData, lba++, 1, buf))
			break;
		
		usbEpTx(EP_NUM_IN, buf, mMsc.blockSz);
		numDone++;
	}
	
	return numDone;
}

static uint32_t mscPrvWrite(uint32_t lba, uint32_t nSec)
{
	uint8_t buf[mMsc.blockSz + EP_PACKET_SZ];
	uint32_t numDone = 0;
	int32_t have, len;
	
	while (nSec--) {
		
		for (have = 0; have < mMsc.blockSz; have += len) {
			
			while ((len = usbEpRx(EP_NUM_OUT, buf + have, EP_PACKET_SZ)) < 0);
		}
		
		if (1 != mMsc.writeF(mMsc.cbkData, lba++, 1, buf))
			break;
		
		memcpy(buf, buf + mMsc.blockSz, have - mMsc.blockSz);
		
		numDone++;
	}
	
	return numDone;
}

static bool mscPrvScsiCmdHandle(const uint8_t *cmd, unsigned len, uint32_t transferLen, uint32_t *dataDoneP)
{
	uint32_t lba = 0, nSec = 0;
	uint8_t buf[64] = {};
	
	if (mMsc.sense == SCSI_SENSE_NO_SENSE)
		mMsc.senseAdditional = SCSI_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
	
	switch (cmd[0]) {
		
		case 0x00:	//test unit ready
			return true;
		
		case 0x03:	//request sense
			buf[0] = 0xF0;     //SCSI-compliant current error
			buf[2] = mMsc.sense;
			buf[12] = mMsc.senseAdditional;
			*dataDoneP = 19;
			usbEpTx(EP_NUM_IN, buf, 19);
			return true;
		
		case 0x12:	//inquiry
			
			if ((cmd[1] & 3) == 0) {
				
				if (transferLen > 36)
					transferLen = 36;
				
				buf[0] = 0;			//currently connected magnetic disk, SBC instruction set
	            buf[1] = 0x80;			//medium is removable (will help windows decide not to do write-caching)
	            buf[2] = 0x04;			//device complies with SPC-2 standard only
	            buf[3] = 0x02;			//no async reporting support, no NACA bit support, no hierarchical LUN assignement support, this response data is format v2
	            buf[4] = 31;			//size of data past here
	
				buf[8] = 'D';			//vendor
				buf[9] = 'm';
				buf[10] = 'i';
				buf[11] = 't';
				buf[12] = 'r';
				buf[13] = 'y';
				buf[14] = 'G';
				buf[15] = 'R';
				
				buf[16] = 'e';			//product
				buf[17] = 'I';
				buf[18] = 'n';
				buf[19] = 'k';
				buf[20] = ' ';
				buf[21] = 'C';
				buf[22] = 'o';
				buf[23] = 'n';
				buf[24] = 't';
				buf[25] = 'r';
				buf[26] = 'o';
				buf[27] = 'l';
				buf[28] = 'l';
				buf[29] = 'e';
				buf[30] = 'r';
				buf[31] = ' ';
				
				buf[32] = '0';			//version
				buf[33] = '.';
				buf[34] = '9';
				buf[35] = '9';
				
				*dataDoneP = transferLen;
				usbEpTx(EP_NUM_IN, buf, transferLen);
				return true;
			}
			else if ((cmd[1] & 3) == 1 && cmd[2] == 0x80) {	//serial number page
				buf[0] = 0;
				buf[1] = 0x80;	//page 0x80
				//length is zero - we have no serial number to report
				
				*dataDoneP = 4;
				usbEpTx(EP_NUM_IN, buf, 4);
				return true;
			}
			else {
				mMsc.sense = SCSI_SENSE_ILLEGAL_REQUEST;
				mMsc.senseAdditional = SCSI_ASC_INVALID_FIELD_IN_CDB;
				return false;
			}
		
		case 0x1a:	//mode sense
			if ((cmd[2] & 0x3f) == 0x3f) {		//all pages or subpages
				buf[0] = 11;		//bytes not including this len field
				buf[2] = 0x00;	//0x80 is read only
				buf[3] = 8;		//block descr len
				buf[5] = mMsc.numBlocks >> 16;
				buf[6] = mMsc.numBlocks >> 8;
				buf[7] = mMsc.numBlocks >> 0;
				buf[8] = mMsc.blockSz >> 16;
				buf[9] = mMsc.blockSz >> 8;
				buf[10] = mMsc.blockSz >> 0;
				*dataDoneP = 12;
				usbEpTx(EP_NUM_IN, buf, 12);
				return true;
			}
			else if ((cmd[2] & 0x3f) == 0x1c) {	//error reporting
				
				buf[0] = 0x1c;		//page 0x1c
				buf[1] = 0x0a;		//page length
				
				*dataDoneP = 12;
				usbEpTx(EP_NUM_IN, buf, 12);
				return true;
			}
			else if ((cmd[2] & 0x3f) == 0x08) {	//caching
				
				buf[0] = 0x08;		//page 0x08
				buf[1] = 0x12;		//page length
				
				*dataDoneP = 20;
				usbEpTx(EP_NUM_IN, buf, 20);
				return true;
			}
			else {
				pr("mode sense %02x %02x\n", cmd[2], cmd[3]);
				mMsc.sense = SCSI_SENSE_ILLEGAL_REQUEST;
				mMsc.senseAdditional = SCSI_ASC_INVALID_FIELD_IN_CDB;
				return false;
			}
		
		case 0x1b:	//start/stop unit
			return true;
		
		case 0x1e:	//allow/prevent menium removal
			return true;
		
		case 0x23:	//read format capacity
			buf[3] = 8; //one descriptors for formattable size
			
			//maximum formattable size
			buf[4] = mMsc.numBlocks >> 24;
			buf[5] = mMsc.numBlocks >> 16;
			buf[6] = mMsc.numBlocks >> 8;
			buf[7] = mMsc.numBlocks >> 0;
			buf[8] = 2;	//this is formatted size
			buf[9] = mMsc.blockSz >> 16;
			buf[10] = mMsc.blockSz >> 8;
			buf[11] = mMsc.blockSz >> 0;
			
			//the one descriptor
			buf[12] = mMsc.numBlocks >> 24;
			buf[13] = mMsc.numBlocks >> 16;
			buf[14] = mMsc.numBlocks >> 8;
			buf[15] = mMsc.numBlocks >> 0;
			buf[16] = 0;	//block size and number
			buf[17] = mMsc.blockSz >> 16;
			buf[18] = mMsc.blockSz >> 8;
			buf[19] = mMsc.blockSz >> 0;
			
			*dataDoneP = 20;
			usbEpTx(EP_NUM_IN, buf, 20);
			return true;
		
		case 0x25:	//read capacity
			buf[0] = (mMsc.numBlocks - 1) >> 24;
			buf[1] = (mMsc.numBlocks - 1) >> 16;
			buf[2] = (mMsc.numBlocks - 1) >> 8;
			buf[3] = (mMsc.numBlocks - 1) >> 0;
			buf[4] = mMsc.blockSz >> 16;
			buf[5] = mMsc.blockSz >> 16;
			buf[6] = mMsc.blockSz >> 8;
			buf[7] = mMsc.blockSz >> 0;
			*dataDoneP = 8;
			usbEpTx(EP_NUM_IN, buf, 8);
			return true;
		
		case 0x28:	//read
			lba = (lba << 8) + cmd[2];
			lba = (lba << 8) + cmd[3];
			lba = (lba << 8) + cmd[4];
			lba = (lba << 8) + cmd[5];
			nSec = (nSec << 8) + cmd[7];
			nSec = (nSec << 8) + cmd[8];
			*dataDoneP = mMsc.blockSz * mscPrvRead(lba, nSec);
			return true;
		
		case 0x2a:	//write
			lba = (lba << 8) + cmd[2];
			lba = (lba << 8) + cmd[3];
			lba = (lba << 8) + cmd[4];
			lba = (lba << 8) + cmd[5];
			nSec = (nSec << 8) + cmd[7];
			nSec = (nSec << 8) + cmd[8];
			*dataDoneP = mMsc.blockSz * mscPrvWrite(lba, nSec);
			return true;
		
		case 0x2f:	//verify
			return true;	//yup...
		
		case 0x56:	//reserve
		case 0x57:	//release
			return true;
		
		case 0xa8:	//read 12
			lba = (lba << 8) + cmd[2];
			lba = (lba << 8) + cmd[3];
			lba = (lba << 8) + cmd[4];
			lba = (lba << 8) + cmd[5];
			nSec = (nSec << 8) + cmd[6];
			nSec = (nSec << 8) + cmd[7];
			nSec = (nSec << 8) + cmd[8];
			nSec = (nSec << 8) + cmd[9];
			*dataDoneP = mMsc.blockSz * mscPrvRead(lba, nSec);
			return true;
		
		case 0xaa:	//write 12
			lba = (lba << 8) + cmd[2];
			lba = (lba << 8) + cmd[3];
			lba = (lba << 8) + cmd[4];
			lba = (lba << 8) + cmd[5];
			nSec = (nSec << 8) + cmd[6];
			nSec = (nSec << 8) + cmd[7];
			nSec = (nSec << 8) + cmd[8];
			nSec = (nSec << 8) + cmd[9];
			*dataDoneP = mMsc.blockSz * mscPrvWrite(lba, nSec);
			return true;
	}
	
	pr("not sure how to handle %u-byte SCSI cmd 0x%02x\n", len, cmd[0]);
	return false;
}

void mscProcess(void)
{
	uint8_t pkt[EP_PACKET_SZ], resp[13], cmdLen;
	uint32_t reqdLen, doneLen = 0;
	int32_t len;
	
	//pr("s %d--\r", mMsc.state);
	
	if (mMsc.state == MscStateInited) {
		if (!usbIsInited())
			return;
		mMsc.state = MscStateUsbReady;
	}
	
	len = usbEpRx(EP_NUM_OUT, pkt, EP_PACKET_SZ);
	if (len < 0)
		return;
	
	if (0) {
		int i;
		
		pr("Got packet (%u b): ", (unsigned)len);
		for (i = 0; i < len; i++)
			pr(" %02x", pkt[i]);
		pr("\n");
	}
	
	if (len != 31) {
		pr("command not 31 bytes\n");
		return;
	}
	
	if (((uint32_t*)pkt)[0] != 0x43425355) {
		pr("command tag fail\n");
		return;
	}
	((uint32_t*)resp)[0] = 0x53425355;				//SIG
	((uint32_t*)resp)[1] = ((const uint32_t*)pkt)[1];		//TAG
	
	if (pkt[13] & 0x0f) {							//LUN: must be zeor for us
		pr("LUN nonzero\n");
		return;
	}
	
	cmdLen = pkt[14] & 0x1f;
		
	if (cmdLen < 0 || cmdLen > 16) {
		pr("cbwcbLen inval\n");
		return;
	}
	
	if (0) {
		int i;
		
		pr("CMD (for LUN %d, %u b): ", pkt[13] & 0x0f, cmdLen);
		for (i = 0; i < cmdLen; i++)
			pr(" %02x", pkt[15 + i]);
		pr("\n");
	}
	
	reqdLen = ((const uint32_t*)pkt)[2];
	resp[12] = mscPrvScsiCmdHandle(pkt + 15, cmdLen, reqdLen, &doneLen) ? 0 : 1;
	((uint32_t*)resp)[2] = reqdLen - doneLen;				//data residue
	
	if (0) {
		int i;
		
		len = sizeof(resp);
		pr("Sent reply (%u b): ", (unsigned)len);
		for (i = 0; i < len; i++)
			pr(" %02x", resp[i]);
		pr("\n");
	}
	
	usbEpTx(EP_NUM_IN, resp, sizeof(resp));
}