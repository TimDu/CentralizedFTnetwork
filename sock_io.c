#include "helpers.h"

/*
 * Function that reads receiving message
 *
 * Returns:
 * 	size read from receive buffer on success; -1 otherwise.
 */
int read_to_container(char *buff, int size, MessageContainer *message) {
	if (size < 8) {
		printf("\nIllegal size of received message.\n");
		return -1;
	}
	int index = 0, i;
	uint32_t receive_int = 0;
	
	// Read message type (little ending)
	for (i = 0; i < 4; ++i) {
		receive_int += (uint32_t) ((buff[i] & 0xff) << (i * 8));
	}
	message->type = (MessageType)receive_int;
	
	// Read message data length (little ending)
	index += i;
	receive_int = 0;
	for (i = 0; i < 4; ++i) {
		receive_int += (uint32_t) ((buff[index + i] & 0xff) << (i * 8));
	}
	message->length = receive_int;
	
	// Check message integrity
	if (size < ( message->length + 8)) {
		printf("\nInvalid receive buffer size (too small).\n");
		return -1;
	}
	
	// Read data
	index += i;
	message->data = (char*)malloc(message->length * sizeof(char));
	for (i = 0; i < message->length; ++i) {
		message->data[i] = buff[index + i];
	}
	
	return message->length + 8 * sizeof(char);
}

/*
 * Method that writes send buffer
 *
 * Returns:
 *	size of message written to send buffer; -1 otherwise
 */
int write_to_buffer(MessageContainer message, char *buff, int size) {
	if (size < (message.length + 8)) {
		printf("\nInvalid send buffer size (too small).\n");
		return -1;
	}
	int index = 0, i, round;
	uint32_t write_int = (uint32_t)message.type;
	char *data = (char*)&write_int;
	
	// Write message type & data length
	for (round = 0; round < 2; ++round) {
		for (i = 0; i < 4; ++i) {
			buff[index + i] = data[i];
		}
		index += i;
		data = (char*)&message.length;
	}
	
	// Write data
	for (i = 0; i < message.length; ++i) {
		buff[index + i] = message.data[i];
	}
	
	return message.length + 8 * sizeof(char);
}
