#ifndef CLIENT_H
#define CLIENT_H

#include "helpers.h"

extern const unsigned int CLIENT_LISTENQ;

// ==== Candidate list utility ====
typedef struct Candidate_IP_Node {
	struct sockaddr_in address;
	struct Candidate_IP_Node *prev;
	struct Candidate_IP_Node *next;
} Candidate_IP_Node;
// ================================

typedef struct {
	char is_cleared;
	char *hostname;
	int fd;
	long offset;
	struct sockaddr_in *address;	// Point to an entry in candidate list
} Connected_Node;

typedef struct {
	char *fname;
	uint32_t size;
	long cur_offset;
	long offset[MAX_CONNECTION];
} Download_Task;

void client_proc(int port);

#endif
