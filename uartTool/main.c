#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "transport.h"

/*
	flash writing is disabled in our bootloader, thus:
	
	we use exec() primitive.
	
	at call time:

	r4 = cmdptr
	r0 = addr
	
	
	qspiWrite@0x6E9 ;(u32 qspiAddr, void* ramSrc, u32 len)
	
	we store code itself at				0x20110000
	we store destination eeprom addr at	0x201100fc
	we buffer 256 bytes at				0x20110100
	
	so the veneer we need is:
	start:
		a13f		add r1, pc, #0xfc		//point to start + 0x100
		483e		ldr r0, [pc, #0xf8]		//read from start + 0xfc
		f240 1200	movw r2, #0x100
		f240 63e9	movw r3, #0x6e9
		4718		bx  r3
	
	thus:
		3f a1 3e 48 40 f2 00 12 40 f2 e9 63 18 47

*/


#define VERBOSE

#ifdef VERBOSE
	#define logverbose(...)		fprintf(stderr, __VA_ARGS__)
#else
	#define logverbose(...)
#endif

static bool mToggleFlag = true;
static uint32_t mPassword = 0xffffffff;

struct ReqPacket {
	uint8_t cmd;
	uint8_t toggler;
	uint8_t len;
	uint32_t passwd;
	uint32_t addr;
} __attribute((packed));

#define CMD_MMIO_READ			2
#define CMD_MMIO_WRITE			3
#define CMD_EEPROM_READ			4
#define CMD_EXEC				5
#define CMD_RESET				7
#define MAX_READ_LEN			128
#define MAX_WRITE_LEN			64
#define FLASH_PAGE_LEN			256

#define FLASH_SIZE				(512 * 1024)
#define ROM_SIZE				(128 * 1024)

#define EE_HELPER_ADDR			0x20110000
#define EE_DEST_ADDR			0x201100fc
#define EE_STAGED_DATA			0x20110100

static void showBytes(const void* bytes, uint32_t len)
{
	const uint8_t *b = (const uint8_t*)bytes;
	uint32_t i;
	
	for (i = 0; i < len; i++) {
		if ((!i & 15))
			logverbose("\n %05x: ", i);
		logverbose(" %02x", *b++);
	}
	logverbose("\n");
}

static uint64_t getMsec(void)
{
	struct timespec tm;
	uint64_t ret;

    clock_gettime(CLOCK_REALTIME, &tm);
	ret = tm.tv_sec * 1000ULL;
	ret += (tm.tv_nsec + 500000) / 1000000;

	return ret;
}

static void showProgress(FILE* stream, const char* label, uint32_t start, uint32_t now, uint32_t end)
{
	uint32_t i, percent = (((uint64_t)(now - start)) * 100 + (end - start) / 2) / (end - start);
	uint64_t time = getMsec();
	static uint64_t startTime;
	
	if (now == start)
		startTime = time;
	
	time -= startTime;
	
	fprintf(stream, "%s 0x%08x..0x%08x..0x%08x |", label, start, now, end);
	for (i = 0; i < 100; i += 5)
		fputc(i < percent ? '=' : ' ', stream);
	fprintf(stream, "| %3u%% ", percent);
	
	if (time) {
		
		uint32_t speed = (now - start) * 1000ULL / time;
		
		fprintf(stream, "%u B/s ", speed);
		if (speed)
			fprintf(stream, "%u sec left ", (end - now + speed / 2) / speed);
	}
	
	fprintf(stream, "        \r");
}		

static void waitForDevice(struct Transport *xport)
{
	static const char ack[3] = "OK0";
	uint8_t byte;
	unsigned i;
	
	fprintf(stderr, "waiting for boot\n");
	while (xport->read(xport, &byte, 1) != 1 || (byte != 0x3a && byte != 0x23));
	
	if (byte == 0x3a) {
		xport->write(xport, &byte, 1);
		fprintf(stderr, "device halted\n");
	}
	else {
		xport->write(xport, &byte, 1);
		fprintf(stderr, "device was waiting\n");
	}
	
	for (i = 0; i < sizeof(ack); i++) {
		while (xport->read(xport, &byte, 1) != 1);
		if (byte != ack[i]) {
			fprintf(stderr, "failed to getck ack[0]\n");
			abort();
		}
	}
	fprintf(stderr, "device halt ACKed\n");
}

static void sendReq(struct Transport *xport, uint8_t cmd, uint8_t len, uint32_t addr, const void *data, uint32_t dataLen)
{
	struct ReqPacket pkt = {.cmd = cmd, .toggler = mToggleFlag ? 0x40 : 0, .len = len, .addr = addr, .passwd = mPassword, };
	uint8_t totalLen = sizeof(pkt) + dataLen;
	
	mToggleFlag = !mToggleFlag;

	if (1 != xport->write(xport, &totalLen, 1) || sizeof(pkt) != xport->write(xport, &pkt, sizeof(pkt)) || dataLen != xport->write(xport, data, dataLen)) {
		fprintf(stderr, "tx error\n");
		abort();
	}
}

static int getResp(struct Transport *xport, void* dstP, uint8_t cmd)
{
	uint8_t len, *dst = (uint8_t*)dstP;
	int i;
	
	while (xport->read(xport, &len, 1) != 1);
	for (i = 0; i < len;) {
		
		int now = len - i;
		
		i += xport->read(xport, &dst[i], len);
	}
	
	if (!(dst[1] & 0x80)) {
		
		fprintf(stderr, "reply lacked proper high bit set\n");
		abort();
	}
	
	if (!(dst[1] & 0x40) == !mToggleFlag) {
		
		fprintf(stderr, "reply lacked proper toggle bit\n");
		abort();
	}
	
	if (dst[0] != cmd) {
		
		fprintf(stderr, "reply packet was for the wrong command\n");
		abort();
	}
	
	return len;
}

static void deviceRead(struct Transport *xport, bool eeprom, uint8_t* dst, uint32_t from, uint32_t len)
{
	uint8_t resp[256];
	int retLen, eLen = len;
	
	if (len > MAX_READ_LEN || !len)
		abort();
	
	eLen += 3;
	eLen /= 4;
	
	sendReq(xport, eeprom ? CMD_EEPROM_READ : CMD_MMIO_READ, eLen, from, NULL, 0);
	retLen = getResp(xport, resp, eeprom ? CMD_EEPROM_READ : CMD_MMIO_READ);
	
	if (retLen != 3 + eLen * 4 || resp[2] != eLen) {
		
		fprintf(stderr, "read reply packet was wrong of a size\n");
		abort();
	}
	
	memcpy(dst, resp + 3, len);
}

static void remoteExecute(struct Transport *xport, uint32_t addr)
{
	uint8_t resp[256];
	sendReq(xport, CMD_EXEC, 0, addr, NULL, 0);
	
	if (3 != getResp(xport, resp, CMD_EXEC)) {
		
		fprintf(stderr, "remote exec failed\n");
		abort();
	}
}

static void eepromErase(struct Transport *xport)
{
	remoteExecute(xport, 0x667);
}

static void deviceReset(struct Transport *xport)
{
	uint8_t resp[256];
	sendReq(xport, CMD_RESET, 0, 0, NULL, 0);
}

static void mmioWrite(struct Transport *xport, const uint8_t* src, uint32_t dstAddr, uint32_t len)
{
	uint8_t resp[256];
	int retLen, eLen = len;
	
	if (len > MAX_WRITE_LEN || !len)
		abort();
	
	eLen += 3;
	eLen /= 4;
	
	sendReq(xport, CMD_MMIO_WRITE, eLen, dstAddr, src, len);
	retLen = getResp(xport, resp, CMD_MMIO_WRITE);
	
	if (retLen != 3 || resp[2] != eLen) {
		
		fprintf(stderr, "write reply packet was wrong of a size\n");
		abort();
	}
}

static void eeWrite(struct Transport *xport, const uint8_t* src, uint32_t dstAddr, uint32_t len)
{
	uint8_t buf[FLASH_PAGE_LEN];
	uint32_t i, now;
	
	if (!len || len > FLASH_PAGE_LEN || (dstAddr + len - 1) / FLASH_PAGE_LEN != dstAddr / FLASH_PAGE_LEN) {
		fprintf(stderr, "invalid eeprom write req: 0x%08x + 0x%08x\n", dstAddr, len);
		abort();
	}
	
	memset(buf, 0xff, FLASH_PAGE_LEN);
	memcpy(buf + (dstAddr % FLASH_PAGE_LEN), src, len);
	
	mmioWrite(xport, (const void*)&dstAddr, EE_DEST_ADDR, 4);
	
	for (i = 0; i < FLASH_PAGE_LEN; i += now) {
		
		now = FLASH_PAGE_LEN - i;
		if (now > MAX_WRITE_LEN)
			now = MAX_WRITE_LEN;
		mmioWrite(xport, buf + i, EE_STAGED_DATA + i, now);
	}
	remoteExecute(xport, EE_HELPER_ADDR);
}

static void eepromWritePrepare(struct Transport *xport)
{
	static const uint8_t helper[] = {0x3f, 0xa1, 0x3e, 0x48, 0x40, 0xf2, 0x00, 0x12, 0x40, 0xf2, 0xe9, 0x63, 0x18, 0x47};
	static bool done = false;
	
	if (done)
		return;
	
	mmioWrite(xport, helper, EE_HELPER_ADDR, sizeof(helper));
	done = true;
}

static void usage(const char *self)
{
	fprintf(stderr,
		"USAGE: %s /dev/tty$WHATEVR <command and options>\n"
		" COMMANDS:\n"
		"  eeread [from [len]] > file\n"
		"    reads from eeprom to stdout\n"
		"  memread [from [len]] > file\n"
		"    reads from MMIO to stdout\n"
		"  eewrite [from [len]] < file\n"
		"    writes to eeprom from stdin. chip should be erased\n"
		"  memwrite [from [len]] < file\n"
		"    writes to MMIO from stdin\n"
		"  eeerase\n"
		"    erase eeprom\n"
		"  updateDmitry < file\n"
		"    update a tag already flashed as DmitryGR build\n"
		"  makeDmitry < file\n"
		"    flash a stock tag to DmitryGR build\n"
		, self);
	
	exit(-1);
}

static int64_t readNum(const char *num)
{
	if (!num || !*num)
		return -1;
	
	if (num[0] == '0' && num[1] == 'x')
		return num[2] ? (int64_t)strtoull(num + 2, NULL, 16) : -1;
	
	return strtoull(num, NULL, 10);
}

static void writeFromStdin(struct Transport *xport, bool ee, uint32_t from, uint32_t len, uint32_t step)
{
	uint32_t i, now;
	
	if (ee)
		eepromWritePrepare(xport);
	
	for (i = 0; i < len; i += now) {
		uint8_t buf[step];
		
		now = len - i;
		if (now > step)
			now = step;
		
		if (now != fread(buf, 1, now, stdin))
			abort();
		
		if (ee)
			eeWrite(xport, buf, from + i, now);
		else
			mmioWrite(xport, buf, from + i, now);
		
		showProgress(stderr, "WRITE", from, from + i, from + len);
	}
}

static void dmitryFlash(struct Transport *xport, bool firstTime)
{
	uint8_t curMac[8];
	int i, and = 0xff, or = 0x00;
	
	//read mac
	deviceRead(xport, true, curMac, firstTime ? 0x74004 : 0x6c000, sizeof(curMac));
	
	//sanity check it
	for (i = 0; i < sizeof(curMac); i++) {
		and &= curMac[i];
		or |= curMac[i];
	}
	if (and == 0xff || or == 0x00) {
		
		fprintf(stderr, "Current mac has no bits!\n");
		exit(-2);
	}
	if (curMac[7] != 0xc8 || curMac[6] != 0xba || curMac[5] != 0x94) {
		
		fprintf(stderr, "Current mac is not from a SoluM device!\n");
		exit(-2);
	}

	fprintf(stderr, "CURRENT MAC: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", curMac[7], curMac[6], curMac[5], curMac[4], curMac[3], curMac[2], curMac[1], curMac[0]);
	
	fprintf(stderr, "ERASING...\n");
	eepromErase(xport);
	fprintf(stderr, "  DONE\n");
	
	eepromWritePrepare(xport);
	
	fprintf(stderr, "SETTING MAC...\n");
	eeWrite(xport, curMac, 0x6c000, sizeof(curMac));
	fprintf(stderr, "  DONE\n");
	
	fprintf(stderr, "WRITING CODE...\n");
	writeFromStdin(xport, true, 0, 0x10000, FLASH_PAGE_LEN);
	fprintf(stderr, "  DONE\n");
	
	fprintf(stderr, "RESETTING...\n");
	deviceReset(xport);
	fprintf(stderr, "  DONE\n");
}

int main(int argc, char** argv)
{
	uint32_t i, from = 0, len = 0;
	struct Transport *xport;
	
	if (argc < 3|| !(xport = transportOpen(argv[1])))
		usage(argv[0]);
	
	if (!strcmp(argv[2], "eeread") || !strcmp(argv[2], "memread") || !strcmp(argv[2], "eewrite") || !strcmp(argv[2], "memwrite")) {
		
		int64_t t;
		
		if (argc >= 4) {
			t = readNum(argv[3]);
			if (t < 0)
				usage(argv[0]);
			from = t;
		}
		if (argc >= 5) {
			t = readNum(argv[4]);
			if (t <= 0)
				usage(argv[0]);
			len = t;
		}
	}
	else if (!strcmp(argv[2], "eeerase") || !strcmp(argv[2], "makeDmitry") || !strcmp(argv[2], "updateDmitry")) {
		
		//nothing
	}
	else
		usage(argv[0]);
	
	waitForDevice(xport);
	
	if (!strcmp(argv[2], "eeread") || !strcmp(argv[2], "memread")) {
		
		bool ee = !strcmp(argv[2], "eeread");
		uint32_t now, max, dfltmax;
		
		max = ee ? FLASH_SIZE -1 : 0xffffffff;
		dfltmax = ee ? FLASH_SIZE : ROM_SIZE;
		
		if (!len)
			len = dfltmax - from;
		
		if (from >= max || max - from < len - 1)
			usage(argv[0]);
		
		for (i = 0; i < len; i += now) {
			uint8_t buf[MAX_READ_LEN];
			
			now = len - i;
			if (now > MAX_READ_LEN)
				now = MAX_READ_LEN;
			
			deviceRead(xport, ee, buf, from + i, now);
			
			if (now != fwrite(buf, 1, now, stdout))
				abort();
			
			showProgress(stderr, "READ", from, from + i, from + len);
		}
	}
	if (!strcmp(argv[2], "eewrite") || !strcmp(argv[2], "memwrite")) {
		
		uint32_t now, max, step;
		bool ee = !strcmp(argv[2], "eewrite");
		
		max = ee ? FLASH_SIZE : ROM_SIZE;
		step = ee ? FLASH_PAGE_LEN : MAX_WRITE_LEN;
		
		if (!len)
			len = max - from;
		
		if (from > max || max - from < len)
			usage(argv[0]);
		
		writeFromStdin(xport, ee, from, len, step);
	}
	else if (!strcmp(argv[2], "eeerase")) {
		
		eepromErase(xport);
		fprintf(stderr, "erase done\n");
	}
	else if (!strcmp(argv[2], "makeDmitry")) {
		
		dmitryFlash(xport, true);
	}
	else if (!strcmp(argv[2], "updateDmitry")) {
		
		dmitryFlash(xport, false);
	}
	
	xport->close(xport);
	fprintf(stderr, "\n\n");
	
	return 0;
}





