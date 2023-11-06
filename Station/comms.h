#ifndef _COMMS_H_
#define _COMMS_H_

#include <stdbool.h>
#include <stdint.h>
#include "ccm.h"


extern const uint32_t mCommsPresharedKey[];


#define COMMS_MAX_RADIO_WAIT_MSEC		50

#define COMMS_IV_SIZE					(4)		//zeroes except these 4 counter bytes

#define COMMS_RX_ERR_NO_PACKETS			(-1)
#define COMMS_RX_ERR_INVALID_PACKET		(-2)
#define COMMS_RX_ERR_MIC_FAIL			(-3)

#define COMMS_MAX_PACKET_SZ				(127 /* max phy len */ - 21 /* max mac frame with panID compression */ - 2 /* FCS len */ - AES_CCM_MIC_SIZE - COMMS_IV_SIZE)

void commsInit(const uint8_t *myMac, uint32_t ivStart);		//myMac pointer must remain valid, it is not copied

bool commsTx(uint8_t radioIdx, const uint8_t *toMac, const void *packet, uint32_t payloadLen, bool useSharedKey, bool withAck, bool includeSourceMac);
int32_t commsRx(uint8_t *radioIdxP, void *data, uint8_t *fromMac, int8_t *rssiP, uint8_t *lqiP, bool *wasBcastP);	//returns length or COMMS_RX_ERR_*


//externally provided
extern void commsExtKeyForMac(uint32_t *keyP, const uint8_t *mac);

#endif
