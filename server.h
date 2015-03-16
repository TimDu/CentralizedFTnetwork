#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include "helpers.h"

extern const unsigned int LISTENQ;

// ==== Utility for Server_IP_List ====
typedef struct Server_IP_Node{
	char is_ready;
	int fd;
	struct sockaddr_in address;
	struct Server_IP_Node *prev;
	struct Server_IP_Node *next;
} Server_IP_Node;
// ==============================================

int server_proc(int port);

#endif
