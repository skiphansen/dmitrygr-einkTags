#include "transport.h"
#include <termios.h>
#include <unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>




//#define LOG_SERIAL


struct state {
	struct termios orig;
	int port;
};


static void tClose(struct Transport *xport)
{
	struct state *state = (struct state*)xport->transportData;
	int port = state->port;
	
	tcsetattr(port, TCSANOW, &state->orig);
	close(port);
	free(xport->transportData);
	free(xport);
}

static int32_t tRead(struct Transport *xport, void* dst, uint32_t len)
{
	struct state *state = (struct state*)xport->transportData;
	int port = state->port;
	uint32_t nRead = len;
	
	uint8_t *b = (uint8_t*)dst;
	
	#ifdef LOG_SERIAL
		fprintf(stderr, ">RX ");
	#endif
	
	while(len) {
		
		#ifdef LOG_SERIAL
		
			ssize_t now = read(port, b, 1);
			fprintf(stderr, " %02x", b[0]);
			
		#else
		
			ssize_t now = read(port, b, len);
		#endif
		
		if (now < 0 && errno == EINTR)
			continue;
		if (now <= 0) {
			fprintf(stderr, "read fail: %d, %d\n", (int)now, errno);
			return -1;
		}
		b += now;
		len -= now;	
	}
	
	#ifdef LOG_SERIAL
		fprintf(stderr, "\n");
	#endif
	
	return nRead;
}

static int32_t tWrite(struct Transport *xport, const void* data, uint32_t len)
{
	struct state *state = (struct state*)xport->transportData;
	const uint8_t *b = (const uint8_t*)data;
	int port = state->port;
	uint32_t nWrote = len;
	
	#ifdef LOG_SERIAL
		fprintf(stderr, ">TX ");
	#endif
	
	while(len) {
		
		#ifdef LOG_SERIAL
			ssize_t now = 1;
			
			fprintf(stderr, " %02x", b[0]);
			
		#else
			ssize_t now = len;
		#endif
		
		now = write(port, b, now);
		if (now < 0 && errno == EINTR)
			continue;
		if (now <= 0) {
			fprintf(stderr, "write fail: %d, %d\n", (int)now, errno);
			return -1;
		}
		b += now;
		len -= now;	
	}
	
	#ifdef LOG_SERIAL
		fprintf(stderr, "\n");
	#endif
	
	return nWrote;
}

bool transportSerialInit(struct Transport *xport, const char* path)
{
	struct state *state;
	struct termios ours;
	int port;
	
	port = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
	
	if (port == -1)
		return false;
	
	state = malloc(sizeof(*state));
	state->port = port;
	
	fcntl(port, F_SETFL, 0);
	
	state->port = port;
	tcgetattr(port, &state->orig);
	tcgetattr(port, &ours);
	
	//115200
	cfsetispeed(&ours, B115200);
	cfsetospeed(&ours, B115200);
	
	//8N1
	ours.c_cflag &=~ (PARENB | CSTOPB | CSIZE);
	ours.c_cflag |= CS8 | CLOCAL | CREAD;

	//sw flow control off
    ours.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL | IUCLC);

	//echo off, raw mode
	ours.c_lflag &=~ (ICANON | ECHO | ECHOE | ISIG);
	ours.c_oflag &= ~OPOST;

	tcsetattr(port, TCSANOW, &ours);
	
	xport->close = &tClose;
	xport->read = &tRead;
	xport->write = &tWrite;
	xport->transportData = state;
	
	return true;
}