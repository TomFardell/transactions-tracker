#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>

// Make repeated send calls to send an buffer of data
void send_data(int sockfd, const char *buffer, size_t buffer_size);
// Make repeated sendfile calls to send a file
void send_file(int sockfd, int pagefd);

// Make repeated recv calls to read data into a buffer until the connection is closed at the other end
ssize_t recv_data(int sockfd, char *buffer, size_t buffer_size);
// Make single recv call returning its length. The passed buffer is zeroed and filled with the received data
ssize_t recv_request(int sockfd, char *buffer, size_t buffer_size);

#endif  // NETWORK_H

// vim: filetype=c :
