#ifndef _FW_H_
#define _FW_H_

#include <stdbool.h>
#include <stdint.h>



//layout-related shit
#define TEXT2		__attribute__((noinline, section(".text2"))) 


//MEMORY
	#define MZ_AVAIL_HEAP_MEM		(0x15F00)
	extern char gHeapMemory[MZ_AVAIL_HEAP_MEM];

//QSPI
	//assumes area is already erased. stock does not use "useQuadProgram"
	bool qspiWrite(bool useQuadProgram, uint32_t addr, const void *data, uint32_t nBytes);	//ROM:00100FF6

	//note, param is NOT address. Direct use not advised, use qspiEraseRange()
	bool qspiEraseSector(uint32_t sectorNum);												//ROM:00100EC0

	//mode does not matter, and 0 should be used. returns num bytes read
	uint32_t qspiRead(uint8_t mode, uint32_t addr, void* dst, uint32_t nBytes);				//ROM:00100D4E

	//in boot rom!
	void qspiChipSleepWake(bool sleep);														//ROM:00000594

//RADIO
	struct TxRequest {
		//all unused fields SHALL be zero, or else...
		uint8_t val0x40;		//must be 0x40
		uint8_t unk1[0x13];		//zeroes please
		uint16_t datarate;		//0 = 802.15.4 standard 250kbps; 1,2,3=marvell custom 500,1000,2000kbps
		uint8_t unk2[6];		//zeroes please
		uint16_t txLen;			//mac and payload, no FCS or phy header included
		uint8_t payload[];
	}__attribute__((packed));
	
	struct RxBuffer {
		struct RxBuffer *links[2];
		uint16_t len;		//includes LQI and RSSI bytes, no FCS
		uint8_t data[];		//followed by {u8 LQI, i8 RSSI}
	}__attribute__((packed));
	
	//return 0 on success. for packets with ack requests, checks for acks and retransmits as needed
	//in that case, success is only reported if an ACK was RXed
	int radioTx(const struct TxRequest *req);												//ROM:0010A512

	//channel numbers are 802.15.4 numbers (11..26)
	void radioSetChannel(uint8_t chNo);														//ROM:00106ECE
	
	//set the short address we expect to see when things come to us
	void radioSetFilterShortAddr(uint16_t addr);											//ROM:0010A168
	
	//set tx power. valid values are 0..40 mapping to -20..+10dBm
	void radioSetTxPower(uint32_t dBm);														//ROM:00101DF4
	
	//set the long address we are
	void radioSetFilterLongAddr(const uint8_t *mac);										//ROM:0010A12A
	
	//set the PAN we expect in packets addressed to us
	void radioSetFilterPan(uint16_t pan);													//ROM:0010A0F8

	//see if theree are any packets in the RX buffer
	bool radioRxAreThereQueuedPackets(void);												//ROM:00106C52
	
	//if there is an RXed packet, grab its buffer, else NULL. Release the packet via radioRxReleaseBuffer()
	struct RxBuffer *radioRxGetNextRxedPacket(void);										//ROM:0010717E
	
	//return a buffer gotten via radioRxGetNextRxedPacket()
	void radioRxReleaseBuffer(struct RxBuffer *buf);										//ROM:001070B0

	//if filter is disabled radio will receive all packets *AND ACK ALL PACKETS*
	void radioRxFilterEnable(bool enabled);

	//load some calibration data
	void radioLoadCalibData(void);															//ROM:00105FA3
	
	//some needed hw init
	void radioEarlyInit(void);																//ROM:00106B64
	
	//without this, RX will not work
	void radioRxQueueInit(void);															//ROM:001070C0

//SCREEN
	extern const void *gEinkLuts[];															//RAM:2010F148
	
	//fow lower power yet!
	void einkPowerOff(void);																//ROM:00104388
	
	//interface (gpios) init
	void einkInterfaceInit(void);															//ROM:00104408
	
	//low level command send
	void einkLlSendCommand(uint8_t cmd);													//ROM:00104348
	
	//low level command data send
	void einkLlSendData(uint8_t byte);														//ROM:001044DE

	//low level byte
	void einkLlRawSpiByte(uint8_t mustBeOne, uint8_t byte);									//ROM:001019FE

	//low level: convert temp ADC measurement to eink temp calibration range
	uint8_t einkLlTempToCalibRange(uint32_t tempAdcVal);									//ROM:00104626

	//low level: send calibration for temp range
	void einkLlSendTempCalib(const void *data);											//ROM:001045DA

//GPIO
	//get the current gpio value. simple
	bool gpioGet(uint32_t idx);
	
	//duh
	void gpioSet(uint32_t idx, bool hi);

//ADC
	//measures battery using bandgap. result is thus:
	//3.14+ -> internal error producing garbage
	//2.95+ -> 207
	//2.85+	-> 223
	//2.76+ -> 239
	//less -> 255
	//we sometimes instead patch this to return raw ADC value: (84000 / voltage)
	uint16_t fwBatteryRawMeasure(void);														//ROM:00100458
	
	//configure the ADC to measure temperature
	void adcConfigTempMeas(void);															//ROM:0010058C

	//perform the actual measurement
	uint32_t adcDoMeasure(void);															//ROM:001005C4

	//save power, be green :)
	void adcOff(void);																		//ROM:0010036C

//POWER
	//sleep in super-low-power for a given duration. device reboots at end (no state preserved)
	//durations up to 131076000 msec (36 hours or so) will work. after that, things get weird
	void __attribute__((noreturn)) sleepForMsec(uint32_t msecFromNow);						///ROM:00109ADC


//MISC
	//printf to uart
	void pr(const char* fmt, ...);															//ROM:001025B0
	void wdtPet(void);																		//ROM:00101BFC

//GENERAL
	extern void (* volatile VECTORS[])(void);												//RAM:20100000 


#endif
