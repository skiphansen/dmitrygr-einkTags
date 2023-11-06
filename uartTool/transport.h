#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_


#include <stdbool.h>
#include <stdint.h>

//interface up

struct Transport;


typedef void (*TransportIfCloseF)(struct Transport *xport);
typedef int32_t (*TransportReadF)(struct Transport *xport, void* data, uint32_t maxLen);
typedef int32_t (*TransportWriteF)(struct Transport *xport, const void* data, uint32_t len);


struct Transport {
	
	TransportIfCloseF close;
	TransportReadF read;
	TransportWriteF write;
	void *transportData;
};

struct Transport *transportOpen(const char *path);



//iface down
bool transportUsbInit(struct Transport *xport, const char* path);
bool transportSerialInit(struct Transport *xport, const char* path);



#endif
