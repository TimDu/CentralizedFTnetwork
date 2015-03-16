#include "client.h"
// 593 
const unsigned int CLIENT_LISTENQ = 10;

struct sockaddr_in my_address;
Candidate_IP_Node *sentry_node, *last_node;

int is_address_equal(char *in, char *comp);
int no_duplicate(Connected_Node*, struct sockaddr_in);
int make_connection(char*, char*);
void send_conn_rej(int fd);
void register_handler(int fd, MessageContainer, Connected_Node*);
MessageContainer prepare_down_message(long offset, uint32_t size, char *);

void append_node_c(Candidate_IP_Node*);
Candidate_IP_Node* remove_node_c(Candidate_IP_Node*);
void clear_list_c();

clock_t start_t, end_t, total_t; // record the start time, end time, and total cost time of downloading

void client_proc(int port) {
	const int on = 1;
	int index;
	char is_exit = 0;
	char is_download = 0;
	
	get_local_address(&my_address);
	my_address.sin_port = htons(port);
	
	// Candidate list initialization
	sentry_node = (Candidate_IP_Node*) malloc(sizeof(Candidate_IP_Node));
	sentry_node->prev = NULL;
	sentry_node->next = NULL;
	last_node = sentry_node;
	
	// Connection list initialization
	unsigned short conn_num = 0;	// Current maximum connection number
	Connected_Node connected_list[MAX_CONNECTION + 1];
	for (index = 0; index < MAX_CONNECTION + 1; ++index) {
		connected_list[index].is_cleared = 1;
	}
	
	// Command line stuff
	char *cmd_buff;
	char *cmd_seg;
	int cmd_size = 20, cmd_read;
	cmd_buff = (char*) malloc((cmd_size + 1) * sizeof(char));
	printf("Welcome! P2P client\n>> ");
	fflush(stdout);
	
	// Download task structure initialization
	Download_Task task_board;
	
	// Create listener socket
	int listener_fd;
	struct sockaddr_in server_address;
	if ((listener_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Failed to initiate listen socket.\n");
		return;
	}
	
	// Set socket address info
	memset((void*)&server_address, 0, sizeof server_address);
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	
	// Set socket option
	if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &on
		, sizeof on)) {
		printf("Failed to set socket to 'SO_REUSEADDR'.\n");
		return;
	}
	
	// Bind and listen
	if (bind(listener_fd, (struct sockaddr*) &server_address
		, sizeof server_address) < 0) {
		printf("Failed to bind socket address to listener.\n");
		return;
	}
	if (listen(listener_fd, CLIENT_LISTENQ) < 0) {
		printf("Failed to listen on server socket.\n");
		return;
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
	fdmax = listener_fd + 1;
	
	// Socket data buffer
	char buffer[MAX_DATA_SIZE];
	int nbytes;	// Bytes read from buffer
	int byte_offset[3], chunck_size;	// Download task recorder
	
	while (!is_exit) {
		read_set_ready = read_set_all;
		if (select(fdmax + 1, &read_set_ready, NULL, NULL, NULL) <= 0) {
			printf("Failed at synchronous socket I/O select.\n");
			return;
		}

		// Check STDIN
		if (FD_ISSET(STDIN_FILENO, &read_set_ready)) {
			// Get command line
			if ((cmd_read = getline(&cmd_buff, &cmd_size, stdin)) > 1) {
				cmd_buff[cmd_read / sizeof(char) - 1] = '\0';
				cmd_seg = strtok(cmd_buff, " ");

				// HELP
				if (!strcmp(cmd_seg, "HELP") || !strcmp(cmd_seg, "help")) {
					if (strtok(NULL, " ") == NULL) {
						printf("Command Options Info:\n");
						printf("HELP---------------------------------------Display Command info\n");
						printf("MYIP---------------------------------------Display IP Address of this host\n");
						printf("MYPORT-------------------------------------Display listening PORT number\n");
						printf("REGISTER<server IP><port no>---------------Connect to server and get IP Addresse\n""%s\n",
						       "                                           s and PORT number of other registered\n"
							   "                                           hosts");
						printf("CONNECT<destination><port no>--------------Connect to other registered host by t\n""%s\n",
						       "                                           heir hostname(or ip addr) and port no");
						printf("LIST---------------------------------------Display a list of connection Info. of\n"
						       "                                           host\n");
						printf("TERMINATE<connection id>-------------------Terminate connection by its id\n");
						printf("EXIT---------------------------------------Close the process and quit\n");
						printf("DOWNLOAD<filename><chunk size in bytes>----Download file by its name and set chu\n"
						       "                                           nk size\n");
					}
					// TODO: more
					else {
						printf("The command should be: HELP.\n");
					}
				}
				
				else if (!strcmp(cmd_seg, "MYIP") || !strcmp(cmd_seg, "myip")) {
					if (strtok(NULL, " ") == NULL) {
						char ip[32];
						int len = sizeof ip / sizeof(char);
						inet_ntop(AF_INET, &(my_address.sin_addr), ip, len);
						printf("Local IP: %s\n", ip);
					}
					// TODO: more
					else {
						printf("The command should be: MYIP.\n");
					}
				}
				else if (!strcmp(cmd_seg, "MYPORT") || !strcmp(cmd_seg, "myport")) {
					if (strtok(NULL, " ") == NULL) {
						printf("Local port: %d\n", ntohs(my_address.sin_port));
					}
					else {
						printf("The command should be: MYPORT.\n");
					}
				}
				else if (!strcmp(cmd_seg, "LIST") || !strcmp(cmd_seg, "list")) {
					if (strtok(NULL, " ") == NULL) {
						char ip[32];
						int len = sizeof ip / sizeof(char);
						int index = 0, counter = 1;
						printf("ID\tHost name\tIP address\tPort No.\n");
						
						for (; index < MAX_CONNECTION + 1; ++index) {
							if (!connected_list[index].is_cleared) {
								inet_ntop(AF_INET
									, &(connected_list[index].address->sin_addr)
									, ip, len);
								
								printf("%d:", counter);
								printf("\t%s", connected_list[index].hostname);
								printf("\t%s", ip);
								printf("\t%d\n", ntohs(
									connected_list[index].address->sin_port));
								
								++counter;
							}
						}
					}
					else {
						printf("The command should be: LIST.\n");
					}
				}
				else if (!strcmp(cmd_seg, "REGISTER") || !strcmp(cmd_seg, "register")) {
					if (connected_list[0].is_cleared) {
						int j;
						char ip[64], t_port[16];
						MessageContainer container;
						
						// Get IP and port
						if ((cmd_seg = strtok(NULL, " ")) == NULL) {
							printf("The command should be: REGISTER [IP] [PORT].\n");
						}
						else {
							strcpy(ip, cmd_seg);
							if ((cmd_seg = strtok(NULL, " ")) == NULL) {
								printf("The command should be: REGISTER [IP] [PORT].\n");
							}
							else {
								strcpy(t_port, cmd_seg);
								// Prepare to register to server
								container.type = REGISTER;
								container.length = 6;
								container.data = (char*) malloc(container.length * sizeof(char));
								for (j = 0; j < 6; ++j) {
									container.data[j] =
										((struct sockaddr*) &my_address)->sa_data[j];
								}
								nbytes = write_to_buffer(container, buffer, MAX_DATA_SIZE);
								if ((connected_list[0].fd = make_connection(ip, t_port)) > 0) {
									send(connected_list[0].fd, buffer, nbytes, 0);
									FD_SET(connected_list[0].fd, &read_set_all);
									if (fdmax < connected_list[0].fd) {
										fdmax = connected_list[0].fd;
									}
									nbytes = recv(connected_list[0].fd, buffer, sizeof buffer, 0);
									MessageContainer m_container;
									if (read_to_container(buffer, nbytes, &m_container) != -1) {
										if (m_container.type == REGISTER) {
											register_handler(connected_list[0].fd
												, m_container, connected_list);
										}
									}
								}
							}
						}
					}
					else {
						printf("Repeat register is not allowed.\n");
					}
				}
				else if (!strcmp(cmd_seg, "CONNECT") || !strcmp(cmd_seg, "connect")) {
					if (conn_num < MAX_CONNECTION) {
						char dest[64], t_port[16];
						if ((cmd_seg = strtok(NULL, " ")) == NULL) {
							printf("The command should be: CONNECT [DEST] [PORT].\n");
						}
						else if (strlen(cmd_seg) > sizeof dest) {
							printf("Destination characters too long.\n");
						}
						else {
							strcpy(dest, cmd_seg);
							Candidate_IP_Node *n = sentry_node->next;
							int new_fd;
							char is_valid = 0;
							
							if ((cmd_seg = strtok(NULL, " ")) == NULL) {
								printf("The command should be: CONNECT [DEST] [PORT].\n");
							}
							else {
								strcpy(t_port, cmd_seg);
								// Check IP Validity
								while (n != NULL) {
									// Self-loop check
									if (is_address_equal(
										((struct sockaddr*) &my_address)->sa_data
										, ((struct sockaddr*) &n->address)->sa_data)) {
										n = n->next;
										continue;
									}
									struct sockaddr_in sa;
									char ip[INET_ADDRSTRLEN];
									
									if (inet_pton(AF_INET, dest, &(sa.sin_addr)) == 1) {
										// IP input
										sa.sin_port = htons(atoi(t_port));										
										if (is_address_equal(((struct sockaddr*) &sa)->sa_data
											, ((struct sockaddr*) &n->address)->sa_data)
											&& no_duplicate(connected_list, sa)) {
											is_valid = 1;
											break;
										}
									}
									else {
										// Host name input
										int j;
										struct hostent *he = gethostbyaddr(&(n->address.sin_addr)
													, sizeof(struct in_addr), AF_INET);
										if (!strcmp(dest, he->h_name)
											&& (atoi(t_port) == (int)ntohs(n->address.sin_port))) {
											is_valid = 1;
											memset(dest, 0, sizeof dest);
											// Convert to IP
											inet_ntop(AF_INET, &(n->address.sin_addr)
												, dest, INET_ADDRSTRLEN);
											break;
										}
									}
									n = n->next;
								}
								if (!is_valid) {
									printf("Invalid connection request.\n");
								}
								else {
									++conn_num;
									int j;
									char *buff;
									MessageContainer container;
									
									// Make container
									container.type = CONNECT_REQ;
									container.length = 6;
									buff = (char*) malloc(container.length * sizeof(char));
									for (j = 0; j < container.length; ++j) {
										buff[j] = ((struct sockaddr*) &my_address)->sa_data[j];
									}
									container.data = buff;
									
									nbytes = write_to_buffer(container, buffer, MAX_DATA_SIZE);								
									if ((new_fd = make_connection(dest, t_port)) > 0) {
										// Find available connection entry
										for (j = 1; j < MAX_CONNECTION + 1; ++j) {
											if (connected_list[j].is_cleared) {
												struct hostent *he =
													gethostbyaddr(&(n->address.sin_addr)
													, sizeof(struct in_addr), AF_INET);
												connected_list[j].is_cleared = 0;
												connected_list[j].hostname = (char*)
													malloc((strlen(he->h_name) + 1) * sizeof(char));
												strcpy(connected_list[j].hostname, he->h_name);
												connected_list[j].fd = new_fd;
												connected_list[j].address = &n->address;
												break;
											}
										}
										// Send connect request
										send(new_fd, buffer, nbytes, 0);
										FD_SET(new_fd, &read_set_all);
										if (fdmax < new_fd) {
											fdmax = new_fd;
										}
									}
								}
							}
						}
					}
					else {
						printf("Reached maximum connection number. Cannot connect.\n");
					}
				}
				else if (!strcmp(cmd_seg, "TERMINATE") || !strcmp(cmd_seg, "terminate")) {
					if ((cmd_seg = strtok(NULL, " ")) == NULL) {
						printf("The command should be: TERMINATE [id].\n");
					}
					else {
						int index, target = atoi(cmd_seg);
						
						// Clear from connection list and read set
						for (index = 0; index < MAX_CONNECTION + 1; ++index) {
							if (!connected_list[index].is_cleared) {
								if ((--target) == 0) {
									connected_list[index].address = NULL;
									free(connected_list[index].hostname);
									FD_CLR(connected_list[index].fd, &read_set_all);
									close(connected_list[index].fd);
									connected_list[index].is_cleared = 1;
									printf("Connection terminated.\n");
									break;
								}
							}
						}
						if (target > 0) {
							printf("Invalid terminate ID.\n");
						}
					}
				}
				else if (!strcmp(cmd_seg, "DOWNLOAD") || !strcmp(cmd_seg, "download")) {
					if ((cmd_seg = strtok(NULL, " ")) == NULL) {
						printf("The command should be: DOWNLOAD [name] [size].\n");
					}
					else {
						
						char *fname = (char*) malloc((strlen(cmd_seg) + 1) * sizeof(char));
						memcpy(fname, cmd_seg, strlen(cmd_seg) + 1);
						if ((cmd_seg = strtok(NULL, " ")) == NULL) {
							printf("The command should be: DOWNLOAD [name] [size].\n");
						}
						else {
							start_t = clock(); // record start time
							int j;
							long off_counter = 0l;
							MessageContainer message;
							
							// Task assignment
							task_board.fname = fname;					
							task_board.size = (uint32_t) atoi(cmd_seg);
							for (j = 0; j < MAX_CONNECTION; ++j) {
								task_board.offset[j] = -1l;
							}
							
							// Prepare & send message
							char l_data[MAX_DATA_SIZE];
							message.type = DOWNLOAD_REQ;
							message.length = sizeof(long) + sizeof(uint32_t)
								+ (strlen(task_board.fname) + 1) * sizeof(char);
							memcpy(&l_data[sizeof(long)], (char*) &task_board.size
								, sizeof(uint32_t));
							strcpy(&l_data[sizeof(long) + sizeof(uint32_t)], task_board.fname);
							message.data = l_data;
							
							is_download = 1;
							FD_CLR(STDIN_FILENO, &read_set_all);
							printf("Downloading %s ...\n", task_board.fname);
							
							// Create the file
							fclose(fopen("file_obj", "wb"));
							
							for (j = 1; j < MAX_CONNECTION + 1; ++j) {
								if (!connected_list[j].is_cleared) {
									memcpy(message.data, (char*) &off_counter, sizeof(long));
								
									nbytes = write_to_buffer(message, buffer, MAX_DATA_SIZE);
									send(connected_list[j].fd, buffer, nbytes, 0);
									connected_list[j].offset = off_counter;
									++off_counter;
								}
							}
							task_board.cur_offset = off_counter;
						}						
					}
				}
				else if (!strcmp(cmd_seg, "EXIT") || !strcmp(cmd_seg, "exit")) {
					is_download = 0;
					is_exit = 1;
				}				
				else {
					printf("Invalid command input.\n");
				}
			}
			if (!is_download) {
				if (!is_exit) {
					printf(">> ");
					fflush(stdout);
				}
				else {
					printf("bye!\n");
				}
			}
		}
		
		// Check any ready receiving buffer
		int m_fd;
		for (m_fd = 1; m_fd < fdmax + 1; ++m_fd) {
			if (m_fd == STDIN_FILENO) {
				continue;
			}
			if (FD_ISSET(m_fd, &read_set_ready)) {
				if (m_fd == listener_fd) {
					// New connections
					new_fd = accept(listener_fd, NULL, NULL);
					// Check connection number validity
					if (conn_num < MAX_CONNECTION) {
						++conn_num;
						if (new_fd == -1) {
							printf("Failed to accept listening socket.\n");
						}
						else {
							// Check IP validity before adding to connection list
							if ((nbytes = recv(new_fd, buffer
								, sizeof buffer / sizeof(char), 0)) <= 0) {
								// Connection terminated
								close(new_fd);
								continue;
							}
							MessageContainer container;
							if (read_to_container(buffer, nbytes, &container) == -1) {
								close(new_fd);
								continue;
							}
							for (index = 1; index < MAX_CONNECTION + 1; ++index) {
								if (connected_list[index].is_cleared) {
									Candidate_IP_Node *node = sentry_node->next;
									while (node != NULL) {
										if (is_address_equal(container.data
											, ((struct sockaddr*) &node->address)->sa_data)) {
											int j;
											struct hostent *he;
											connected_list[index].fd = new_fd;
											connected_list[index].address =	&node->address;
											he = gethostbyaddr(&(node->address.sin_addr)
												, sizeof(struct in_addr), AF_INET);
											connected_list[index].hostname =
												(char*) malloc((strlen(he->h_name) + 1)
													* sizeof(char));
											strcpy(connected_list[index].hostname
												,he->h_name);
											FD_SET(new_fd, &read_set_all);
											if (fdmax < new_fd) {
												fdmax = new_fd;
											}
											connected_list[index].is_cleared = 0;
											break;
										}
										node = node->next;
									}
									if (node == NULL) {
										// Not valid IP address
										printf("Not valid incoming connection."
											"New connection rejected.\n");
										send_conn_rej(new_fd);
										close(new_fd);
									}
									break;
								}
							}
						}
					}
					else {
						printf("\nMaximum connection reached. New connection request rejected.\n");
						printf(">> ");
						fflush(stdout);
						send_conn_rej(new_fd);
						close(new_fd);
					}
				}
				else {
					if ((nbytes = recv(m_fd, buffer, sizeof buffer, 0)) <= 0) {
						// Consider it as a connection disruption
						int j;
						for (j = 0; j < MAX_CONNECTION + 1; ++j) {
							if (m_fd == connected_list[j].fd) {
								printf("\nConnection to %s is disconnected.\n"
									, connected_list[j].hostname);
								printf(">> ");
								fflush(stdout);
								// Record unfinished download task
								task_board.offset[j] = connected_list[j].offset;
								connected_list[j].is_cleared = 1;
								free(connected_list[j].hostname);
								connected_list[j].address = NULL;
								FD_CLR(m_fd, &read_set_all);
								close(m_fd);
								--conn_num;
								break;
							}
						}
					}
					else {
						MessageContainer m_container;
						
						if (read_to_container(buffer, nbytes, &m_container) != -1) {
							// Message should be integral, process it
							if ((m_container.type == REGISTER)
								&& (m_fd == connected_list[0].fd)) {
								register_handler(m_fd, m_container, connected_list);
							}
							else if (m_container.type == CONNECT_REJ) {
								// Received connection rejection
								printf("\nConnection rejected.\n");
								printf(">> ");
								fflush(stdout);
							}
							else if (m_container.type == DOWNLOAD_REQ) {
								MessageContainer container; // Response message
								container.type = DOWNLOAD_REP;
								container.length = 0;
								
								// Legitimate data read
								int j;
								long offset = 0l;
								uint32_t size = 0;
								char *fname, *f_data;
								for (j = 0; j < sizeof(long); ++j) {
									// Offset assignment
									offset += (long) ((m_container.data[j] & 0xff) << (j * 8));
								}
								for (j = 0; j < sizeof(uint32_t); ++j) {
									// Size assignment
									size +=
										(uint32_t) ((m_container.data[sizeof(long) + j] & 0xff)
										<< (j * 8));
								}
								fname = (char*) malloc(m_container.length
									- sizeof(long) - sizeof(uint32_t));
								strcpy(fname
									, (char*) &m_container.data[sizeof(long) + sizeof(uint32_t)]);
								
								// Read file
								FILE *fp = fopen(fname, "rb");
								fseek(fp, 0, SEEK_END);
								if ((fp != NULL) && (ftell(fp) >= (offset * size))) {
									rewind(fp);
									fseek(fp, offset * (long)size, SEEK_SET);
									f_data = (char*) malloc(size * sizeof(char));
									size = fread(f_data, 1, size, fp);
									fclose(fp);
									// Read succeeds
									container.length
										= sizeof(long) + sizeof(uint32_t) + size;
									container.data =
										(char*) malloc(container.length * sizeof(char));
									// Assign data to message
									memcpy(container.data, m_container.data
										, sizeof(long) + sizeof(uint32_t));
									memcpy(&container.data[sizeof(long) + sizeof(uint32_t)]
										, f_data, size * sizeof(char));
									free(f_data);
								}
								free(fname);
								
								nbytes = write_to_buffer(container, buffer, MAX_DATA_SIZE);
								if (container.length > 0) {
									free(container.data);
								}
								send(m_fd, buffer, nbytes, 0);
							}
							else if (m_container.type == DOWNLOAD_REP) {
								int j, cur_index = 0;
								MessageContainer container;
								
								for (j = 0; j < MAX_CONNECTION + 1; ++j) {
									if (connected_list[j].fd == m_fd) {
										cur_index = j;
										break;
									}
								}
								connected_list[cur_index].offset = -1l;
								
								if (m_container.length > 0) {
									FILE *fp = fopen("file_obj", "r+b");
									long offset = 0l;
									int size = 0, actual_size;
									char *content;
									
									for (j = 0; j < sizeof(long); ++j) {
										offset += (long) ((m_container.data[j] & 0xff) << (j * 8));
									}
									for (j = 0; j < sizeof(uint32_t); ++j) {
										size += (uint32_t) ((m_container.data[
											sizeof(long) + j] & 0xff) << (j * 8));
									}
									
									actual_size =
										m_container.length - sizeof(long) - sizeof(uint32_t);
									printf("Received %d bytes from %s [port #%d].\n", actual_size
										, connected_list[cur_index].hostname
										, ntohs(connected_list[cur_index].address->sin_port));
									
									content = (char*) malloc(actual_size);
									memcpy(content
										, (char*) &m_container.data[sizeof(long) + sizeof(uint32_t)]
										, actual_size);

									fseek(fp, offset * size, SEEK_SET);
									fwrite(content, sizeof(char), actual_size, fp);
									fclose(fp);

									if (actual_size == size) {
										container = prepare_down_message(
											task_board.cur_offset
											,task_board.size, task_board.fname);
										nbytes = write_to_buffer(container, buffer, MAX_DATA_SIZE);
										send(m_fd, buffer, nbytes, 0);
										connected_list[cur_index].offset = task_board.cur_offset;
										++task_board.cur_offset;
										continue;
									}
								}
								
								// Check remained tasks
								for (j = 1; j < MAX_CONNECTION; ++j) {
									if (task_board.offset[j] > -1l) {printf("1???\n");
										container = prepare_down_message(
											task_board.offset[j]
											, task_board.size
											, task_board.fname);
										nbytes = write_to_buffer(
											container, buffer, MAX_DATA_SIZE);
										free(container.data);
										send(m_fd, buffer, nbytes, 0);
										connected_list[cur_index].offset =
											task_board.offset[j];
										task_board.offset[j] = -1l;
										break;
									}
									if (!connected_list[j].is_cleared
										&& (connected_list[j].offset != -1l)) {
										break;
									}
								}
								if (j == MAX_CONNECTION) {
									// Download finish, get STDIN back
									FD_SET(STDIN_FILENO, &read_set_all);
									is_download = 0;
						            end_t = clock();
									total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
									printf("cost %f secs\n", total_t);
									printf(">> ");
									fflush(stdout);
								}
							}
						}
						
						free(m_container.data);
					}
				}
			}
		}
	}
	
	int j;
	Candidate_IP_Node *n;
	free(cmd_buff);
	FD_ZERO(&read_set_all);
	FD_ZERO(&read_set_ready);
	for (j = 0; j < MAX_CONNECTION + 1; ++j) {
		if (!connected_list[j].is_cleared) {
			close(connected_list[j].fd);
			free(connected_list[j].hostname);
			connected_list[j].address = NULL;
		}
	}
	while (sentry_node != NULL) {
		n = sentry_node;
		sentry_node = sentry_node->next;
		free(n);
	}
}

MessageContainer prepare_down_message(long offset, uint32_t size, char *name) {
	MessageContainer message;
	
	message.type = DOWNLOAD_REQ;
	message.length = sizeof(long) + sizeof(uint32_t)
		+ (strlen(name) + 1) * sizeof(char);
	message.data = (char*) malloc(message.length);
	memcpy(message.data, (char*) &offset, sizeof(long));
	memcpy(&message.data[sizeof(long)], (char*) &size
		, sizeof(uint32_t));
	strcpy(&message.data[sizeof(long) + sizeof(uint32_t)], name);
	
	return message;
}

/* Append node to the end of list */
void append_node_c(Candidate_IP_Node *node) {
	last_node->next = node;
	node->prev = last_node;
	last_node = node;
}

/* Remove node from candidate list */
Candidate_IP_Node* remove_node_c(Candidate_IP_Node* node) {
	Candidate_IP_Node *m_next = node->next;
	
	if ((m_next != NULL) && (node->prev != NULL)) {
		node->prev->next = m_next;
		m_next->prev = node->prev;
	}
	else if (node->prev != NULL) {
		node->prev->next = NULL;
		last_node = node->prev;
	}
	else {
		// Cannot remove the sentry node
		return sentry_node->next;
	}
	free(node);
	
	return m_next;
}

/* Clear up candidate list */
void clear_list_c() {
	Candidate_IP_Node *node;
	while (node != NULL) {
		node = remove_node_c(node);
	}
}

int is_address_equal(char *in, char *comp) {
	int i;
	
	for (i = 0; i < 6; ++i) {
		if (in[i] != comp[i]) {
			return 0;
		}
	}
	
	return 1;
}

int no_duplicate(Connected_Node *list, struct sockaddr_in addr) {
	int dup_j;
	
	for (dup_j = 1; dup_j < MAX_CONNECTION + 1; ++dup_j) {
		if (!list[dup_j].is_cleared) {
			if (is_address_equal(((struct sockaddr*) &addr)->sa_data
				, ((struct sockaddr*)list[dup_j].address)->sa_data)) {
				return 0;	
			}
		}
	}
	
	return 1;
}

int make_connection(char *ip, char *port) {
	int fd;
	struct addrinfo hints, *conn_info, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if (getaddrinfo(ip, port, &hints, &conn_info) != 0) {
		printf("Failed to get address info for register.\n");
		return -1;
	}
	
	for (p = conn_info; p != NULL; p = p->ai_next) {
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			continue;
		}
		if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(fd);
			continue;
		}
		break;
	}
	
	freeaddrinfo(conn_info);
	
	if (p == NULL) {
		printf("Failed to make connection with %s.\n", ip);
		return -1;
	}
	
	return fd;
}

void send_conn_rej(int fd) {
	char buff[32];
	int size;
	MessageContainer container;
	
	container.type = CONNECT_REJ;
	container.length = 0;
	size = write_to_buffer(container, buff, sizeof buff / sizeof(char));
	
	send(fd, buff, size, 0);
}

void register_handler(int m_fd, MessageContainer m_container, Connected_Node *connected_list) {
	// Message should be integral, process it
	int byte_index;
	
	// Read updated list from server
	int is_first;
	Candidate_IP_Node *node = sentry_node;

	is_first = (node->next == NULL);
	
	if ((m_container.length % 6) != 0) {
		printf("Invalid server IP list information.\n");
		return;
	}
	for (byte_index = 0; byte_index < m_container.length;
		byte_index += 6) {
		int j;
		struct sockaddr init_addr;
		
		// Read a port and address to sockaddr
		init_addr.sa_family = AF_INET;
		for (j = 0; j < 6; ++j) {
			init_addr.sa_data[j] = m_container.data[byte_index + j];
		}
		if (!is_first) {
			while ((node != NULL) 
				&& !is_address_equal(init_addr.sa_data
				, ((struct sockaddr*) &node->address)->sa_data)) {
				node = remove_node_c(node);
			}
		}
		
		if (is_first || (node == NULL)) {
			struct sockaddr_in *tgt_addr =
				(struct sockaddr_in*) &init_addr;
			if (node == sentry_node) {
				struct hostent *he;
				
				node->address = *tgt_addr;
				connected_list[0].address = &sentry_node->address;
				he = gethostbyaddr(&(node->address.sin_addr)
					, sizeof(struct in_addr), AF_INET);
				connected_list[0].hostname =
					(char*) malloc(
						(strlen(he->h_name) + 1) * sizeof(char));
				strcpy(connected_list[0].hostname, he->h_name);

				node = node->next;
			}
			else {
				Candidate_IP_Node *candi =
					(Candidate_IP_Node*) malloc(sizeof(Candidate_IP_Node));
				candi->next = NULL;
				candi->address = *tgt_addr;
				
				append_node_c(candi);
			}
		}
		else {
			node = node->next;
		}
		
		// Handle second registers
		if (connected_list[0].is_cleared) {
			connected_list[0].address = &sentry_node->address;
			struct hostent *he = gethostbyaddr(&(sentry_node->address.sin_addr)
				, sizeof(struct in_addr), AF_INET);
			connected_list[0].hostname =
				(char*) malloc(
					(strlen(he->h_name) + 1) * sizeof(char));
			strcpy(connected_list[0].hostname, he->h_name);
			connected_list[0].is_cleared = 0;
		}
	}
	while (node != NULL) {
		node = remove_node_c(node);
	}
	if (is_first) {
		connected_list[0].is_cleared = 0;
	}
}
