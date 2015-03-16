#include "helpers.h"

const unsigned int MAX_DATA_SIZE = 2000;

/* Method that retrieves local IPv4 address*/
void get_local_address(struct sockaddr_in *my_address) {
	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);	// NOTE: IPv4 only
	if (sock_fd < 0) {
		printf("Failed to instantiate a UDP socket. "
			"Unable to resolve local IP address.\n");
		return;
	}
	
	// Set up remote server parameters
	const char* remote_ip = "8.8.8.8";	// Google server
	unsigned int remote_port = 53;
	struct sockaddr_in remote_address;
	socklen_t sock_len = sizeof my_address;
	memset(&remote_address, 0, sizeof remote_address);
	remote_address.sin_family = AF_INET;
	inet_pton(remote_address.sin_family, remote_ip, &remote_address.sin_addr);
	remote_address.sin_port = htons(remote_port);
	
	// Make connection
	if (connect(sock_fd, (struct sockaddr*)&remote_address
		, sizeof remote_address) < 0){
		printf("Failed to connect to UDP server. Unable to resolve local"
			"IP address.\n");
		close(sock_fd);
		return;
	}
	
	// Retrieve address information
	if (getsockname(sock_fd, (struct sockaddr*)my_address, &sock_len) < 0) {
		printf("Failed to collect UDP socket information. Unable to" 
			"resolve local IP address.\n");
		close(sock_fd);
		return;
	}
	
	close(sock_fd);
}
