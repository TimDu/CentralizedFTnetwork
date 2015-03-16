#include "server.h"

const unsigned int LISTENQ = 20;
Server_IP_Node *first_node, *last_node;

void broadcast_updated_list();

void append_node(int fd);
Server_IP_Node* remove_node(Server_IP_Node*);
void clear_list();

/* Server routine */
int server_proc(int port) {
	const int on = 1;
	char is_exit = 0;
	
	// Ignore broken socket, let select() handles it
	signal(SIGPIPE, SIG_IGN);
	
	// Command line stuff
	char *cmd_buff;
	char *cmd_seg;
	int cmd_size = 20, cmd_read;
	cmd_buff = (char*) malloc((cmd_size + 1) * sizeof(char));
	printf("Welcome! P2P server\n>> ");
	fflush(stdout);
	
	// Create listener socket
	int listener_fd;
	struct sockaddr_in server_address;
	if ((listener_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Failed to initiate listen socket.\n");
		return 1;
	}
	
	// Set socket address info
	memset((void*)&server_address, 0, sizeof server_address);
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	
	// Set socket options
	if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &on
		, sizeof on)) {
		printf("Failed to set socket to 'SO_REUSEADDR'.\n");
		return 1;
	}
	if (setsockopt(listener_fd, SOL_SOCKET, SO_KEEPALIVE, &on
		, sizeof on)) {
		printf("Failed to set socket to 'SO_KEEPALIVE'.\n");
		return 1;
	}
	
	// Bind and listen
	if (bind(listener_fd, (struct sockaddr*) &server_address
		, sizeof server_address) < 0) {
		printf("Failed to bind socket address to listener.\n");
		return 1;
	}
	if (listen(listener_fd, LISTENQ) < 0) {
		printf("Failed to listen on server socket.\n");
		return 1;
	}
	
	// Initialize select() relative parameters
	int fdmax;	// Maximum file descriptor number
	int rv;	// Status code of select function
	int new_fd;	// New socket file descriptor received
	fd_set read_set_all;
	fd_set read_set_ready;
	
	FD_ZERO(&read_set_all);
	FD_ZERO(&read_set_ready);
	FD_SET(STDIN_FILENO, &read_set_all);
	FD_SET(listener_fd, &read_set_all);
	fdmax = listener_fd;
	
	// Socket data buffer
	char buffer[MAX_DATA_SIZE];
	int nbytes;	// Bytes read from buffer

	// Initialize server IP address list
	struct sockaddr_in serv_addr;
	get_local_address(&serv_addr);
	first_node = (Server_IP_Node*) malloc(sizeof(Server_IP_Node));
	first_node->fd = listener_fd;
	serv_addr.sin_port = htons(port);
	first_node->address = serv_addr;
	first_node->prev = NULL;
	first_node->next = NULL;
	first_node->is_ready = 1;
	last_node = first_node;
	
	while (!is_exit) {
		read_set_ready = read_set_all;
		if (select(fdmax + 1, &read_set_ready, NULL, NULL, NULL) <= 0) {
			printf("Failed at synchronous socket I/O select.\n");
			return 1;
		}

		// Check STDIN
		if (FD_ISSET(STDIN_FILENO, &read_set_ready)) {
			// Get command line
			if ((cmd_read = getline(&cmd_buff, &cmd_size, stdin)) > 1) {
				cmd_buff[cmd_read / sizeof(char) - 1] = '\0';
				cmd_seg = strtok(cmd_buff, " ");

				if (!strcmp(cmd_seg, "MYIP") || !strcmp(cmd_seg, "myip")) {
					if (strtok(NULL, " ") == NULL) {
						char ip[32];
						int len = sizeof ip / sizeof(char);
						inet_ntop(AF_INET, &(serv_addr.sin_addr), ip, len);
						printf("Local IP: %s\n", ip);
					}
					else {
						printf("The command should be: MYIP.\n");
					}
				}
				else if (!strcmp(cmd_seg, "MYPORT") || !strcmp(cmd_seg, "myport")) {
					if (strtok(NULL, " ") == NULL) {
						printf("Local port: %d\n", ntohs(serv_addr.sin_port));
					}
					else {
						printf("The command should be: MYPORT.\n");
					}
				}
				else if (!strcmp(cmd_seg, "LIST") || !strcmp(cmd_seg, "list")) {
					if (strtok(NULL, " ") == NULL) {
						char ip[32];
						int len = sizeof ip / sizeof(char);
						int index = 1;
						struct hostent *he;
						Server_IP_Node *i = first_node;
						printf("ID\tHost name\tIP address\tPort No.\n");
						
						while (i != NULL) {
							if (i->is_ready) {
								he = gethostbyaddr(&(i->address.sin_addr)
									, sizeof(struct in_addr), AF_INET);
								inet_ntop(AF_INET, &(i->address.sin_addr), ip, len);
								
								printf("%d:", index);
								printf("\t%s", he->h_name);
								printf("\t%s", ip);
								printf("\t%d\n", ntohs(i->address.sin_port));
								
								++index;
							}
							i = i->next;
						}
					}
					else {
						printf("The command should be: LIST.\n");
					}
				}
				else if (!strcmp(cmd_seg, "EXIT") || !strcmp(cmd_seg, "exit")) {
					is_exit = 1;
				}
				else {
					printf("Invalid command input.\n");
				}
			}
			if (!is_exit) {
				printf(">> ");
				fflush(stdout);
			}
			else {
				printf("bye!\n");
			}
		}
		
		// Check any ready receiving buffer
		Server_IP_Node *i = first_node;
		while (i != NULL) {
			int m_fd = i->fd;

			if (FD_ISSET(m_fd, &read_set_ready)) {
				if (m_fd == listener_fd) {
					// New connections
					new_fd = accept(listener_fd, NULL, NULL);
					if (new_fd == -1) {
						printf("Failed to accept listening socket.\n");
					}
					else {
						FD_SET(new_fd, &read_set_all);
						if (fdmax < new_fd) {
							fdmax = new_fd;
						}
						append_node(new_fd);
					}
					i = i->next;
				}
				else {
					if ((nbytes = recv(m_fd, buffer, sizeof buffer / sizeof(char), 0)) <= 0) {
						// Consider it as a connection disruption
						i = remove_node(i);
						FD_CLR(m_fd, &read_set_all);
						close(m_fd);
						broadcast_updated_list();
					}
					else {
						MessageContainer m_container;
						if (read_to_container(buffer, nbytes, &m_container) != -1) {
							// Message should be integral, process it
							int byte_index;
							if (m_container.type == REGISTER) {
								struct sockaddr c_address;
								// Message validation check
								if (m_container.length >
									sizeof c_address.sa_data / sizeof(char)) {
									printf("Invalid 'REGISTER' message size.\n");
									i = remove_node(i);
									FD_CLR(m_fd, &read_set_all);
									close(m_fd);
								}
								else {
									// Assign data to temporary address variable
									for (byte_index = 0; byte_index < m_container.length;
										++byte_index) {
										c_address.sa_data[byte_index] =
											m_container.data[byte_index];
									}
									c_address.sa_family = AF_INET;
									// Copy to local address list
									i->address = *((struct sockaddr_in*) &c_address);
									i->is_ready = 1;
									broadcast_updated_list();
									i = i->next;
								}
							}
						}
					}
				}
			}
			else {
				i = i->next;
			}
		}
	}
	
	free(cmd_buff);
	FD_ZERO(&read_set_all);
	FD_ZERO(&read_set_ready);
	while (first_node != NULL) {
		Server_IP_Node *node = first_node->next;
		close(first_node->fd);
		free(first_node);
		first_node = node;
	}
	
	return 0;
}

/* Broadcast updated server IP list */
void broadcast_updated_list() {
	int size = 0, i, j;
	char *buff, result[MAX_DATA_SIZE];
	Server_IP_Node *n = first_node;
	
	while (n != NULL) {
		if (n->is_ready) {
			++size;
		}
		n = n->next;
	}
	
	buff = (char*) malloc(6 * size * sizeof(char));
	
	// Prepare data
	n = first_node;
	i = 0;
	while (n != NULL) {
		if (n->is_ready) {
			for (j = 0; j < 6; ++j) {
				buff[6 * i + j] = ((struct sockaddr*) &n->address)->sa_data[j];
			}
			++i;
		}
		n = n->next;
	}
	
	// Make message
	MessageContainer container;
	container.type = REGISTER;
	container.length = 6 * size *sizeof(char);
	container.data = buff;
	
	// Send data
	size = write_to_buffer(container, result, sizeof result / sizeof(char));
	n = first_node->next;
	while (n != NULL) {
		if (n->is_ready) {
			send(n->fd, result, size, 0);
		}
		n = n->next;
	}
	
	free(buff);
}

/* Append address node to the end of list */
void append_node(int fd) {
	last_node->next = (Server_IP_Node*) malloc(sizeof(Server_IP_Node));
	last_node->next->prev = last_node;
	last_node = last_node->next;
	last_node->is_ready = 0;
	last_node->fd = fd;
	last_node->next = NULL;
}

/* 
 * Remove address node from server list 
 * Returns:
 * 	the next node pointer after the deleted one
 */
Server_IP_Node* remove_node(Server_IP_Node *node) {
	Server_IP_Node *m_next = node->next;
	
	if ((m_next != NULL) && (node->prev != NULL)) {
		node->prev->next = m_next;
		m_next->prev = node->prev;
	}
	else if (node->prev != NULL) {
		node->prev->next = NULL;
		last_node = node->prev;
	}
	else {
		// Cannot remove the first node
		return first_node->next;
	}
	free(node);
	
	return m_next;
}

/* Clear up all nodes in server list */
void clear_list() {
	Server_IP_Node *node;
	while (node != NULL) {
		node = remove_node(node);
	}
}
