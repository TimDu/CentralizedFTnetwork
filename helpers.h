#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_CONNECTION 3

extern const unsigned int MAX_DATA_SIZE;

typedef enum {
	REGISTER = 0,
	CONNECT_REQ = 1,
	CONNECT_REJ = 2,
	DOWNLOAD_REQ = 3,
	DOWNLOAD_REP = 4,
} MessageType;

typedef struct {
	MessageType type;
	uint32_t length; // Data length,
	char *data;
} MessageContainer;

void get_local_address(struct sockaddr_in*);
int read_to_container(char*, int, MessageContainer*);
int write_to_buffer(MessageContainer, char*, int);

#endif
