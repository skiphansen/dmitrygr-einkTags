#ifndef _TI_RADIO_H_
#define _TI_RADIO_H_

#include <stdbool.h>
#include <stdint.h>


#define SUB_GHZ_CH_OFST			100
#define SUB_GHZ_NUM_CHANNELS	25

bool tiRadioInit(void);
bool tiRadioSetChannel(uint_fast8_t channel);	//SUB_GHZ_CH_OFST + .. SUB_GHZ_CH_OFST + 24,. 903 MHz + 1MHz * chNum

bool tiRadioTxLL(const void* pkt);				//waits till done. raw packet incl len


bool tiRadioRxEnable(bool on, bool autoAck);
void tiRadioRxFilterCfg(const uint8_t *filterForLong, uint32_t myShortMac, uint16_t myPan, bool promisc);
void tiRadioRxFlush(void);
int32_t tiRadioRxDequeuePkt(void* dstBuf, uint32_t maxLen, int8_t *rssiP, uint8_t *lqiP);	//size returened. what you did not read gets dropped. negative if none

void tiRadioRxAckReset(void);
int16_t tiRadioRxAckGetLast(void);	//get seq of lask ack we got or -1 if none

void tiRadioTxConfigure(const uint8_t *myLongMac, uint32_t myShortMac, uint16_t pan);



//careful: 802.15.4 length includes CRC, TI does not

#endif
