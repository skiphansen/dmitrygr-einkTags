#ifndef _RADIO_H_
#define _RADIO_H_

#include <stdbool.h>
#include <stdint.h>

#define RADIO_MAX_PACKET_LEN			(127)
#define RADIO_FIRST_VALID_CHANNEL		(11)
#define RADIO_LAST_VALID_CHANNEL		(26)


#define ADDR_MODE_NONE					(0)
#define ADDR_MODE_SHORT					(2)
#define ADDR_MODE_LONG					(3)

#define FRAME_TYPE_BEACON				(0)
#define FRAME_TYPE_DATA					(1)
#define FRAME_TYPE_ACK					(2)
#define FRAME_TYPE_MAC_CMD				(3)

#define SHORT_MAC_UNUSED				(0x10000000UL)	//for radioRxFilterCfg's myShortMac

struct MacFcs {
	uint16_t frameType			: 3;
	uint16_t secure				: 1;
	uint16_t framePending		: 1;
	uint16_t ackReqd			: 1;
	uint16_t panIdCompressed	: 1;
	uint16_t rfu1				: 3;
	uint16_t destAddrType		: 2;
	uint16_t frameVer			: 2;
	uint16_t srcAddrType		: 2;
} __attribute__((packed));

bool radioInit(void);
bool radioSetChannel(uint_fast8_t channel);	//802.15.4 chanel numbers apply (11..26)

bool radioRxEnable(bool on, bool autoAck);
void radioRxFilterCfg(const uint8_t *filterForLong, uint32_t myShortMac, uint16_t myPan, bool promisc);
void radioRxFlush(void);
int32_t radioRxDequeuePkt(void* dstBuf, uint32_t maxLen, int8_t *rssiP, uint8_t *lqiP);	//size returened. what you did not read gets dropped. negative if none

void radioRxAckReset(void);
int16_t radioRxAckGetLast(void);	//get seq of lask ack we got or -1 if none

void radioTxConfigure(const uint8_t *myLongMac, uint32_t myShortMac, uint16_t pan);
bool radioTxLL(const void* pkt);				//waits till done. raw packet incl len





#endif
