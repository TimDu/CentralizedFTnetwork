#include "helpers.h"
#include "server.h"
#include "client.h"
#include <assert.h>

int main(int argc, char *argv[]) {
	/*
	struct sockaddr_in m_addr;
	char ip[32];
	int len = sizeof ip / sizeof(char);
	
	get_local_address(&m_addr);
	inet_ntop(AF_INET, &(m_addr.sin_addr), ip, len);
	printf("Local IP: %s\n", ip);
	printf("%d\n", sizeof(MessageType));*/
	
	/*// ==== Test sock_io ====
	uint32_t num = 3721;
	char *c_num = (char*)&num;
	MessageContainer container, result;
	container.type = REGISTER;
	container.length = 4;
	container.data = c_num;
	char buffer[20];
	int i;
	
	assert(sizeof(uint32_t) == 4);
	printf("Wrote %d bytes.\n", write_to_buffer(container, buffer, 20));
	printf("Read %d bytes.\n", read_to_container(buffer, 20, &result));

	assert(result.type == REGISTER);
	assert(result.length == 4);
	for (i = 0; i < 4; ++i) {
		assert(container.data[i] == result.data[i]);
	}
	// ======================*/
	if (argc != 3) {
		printf("Wrong number of arguments!\n");
	}
	if (!strcmp(argv[1], "s")) {
		server_proc(atoi(argv[2]));
	}
	else if (!strcmp(argv[1], "c")) {
		client_proc(atoi(argv[2]));
	}
	else {
		printf("Invalid argument input.\n");
		
		return 1;
	}
		
	return 0;
}
