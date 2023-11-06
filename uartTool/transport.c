#include "transport.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



struct Transport *transportOpen(const char *path)
{
	struct Transport *xport = malloc(sizeof(struct Transport));
	
	if (xport) {
		
		if (transportSerialInit(xport, path))
			return xport;
		
		fprintf(stderr, "not sure how to open port '%s'\n", path);
	}

	free(xport);
	return NULL;
}
