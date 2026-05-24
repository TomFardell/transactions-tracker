#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>

// Make repeated send calls to send an buffer of data
void send_data(int sockfd, const char *buffer, size_t buffer_size);
// Make repeated sendfile calls to send a file
void send_file(int sockfd, int pagefd);

// Make repeated recv calls to read data into a buffer until the connection is closed at the other end
ssize_t recv_data(int sockfd, char *buffer, size_t buffer_size);
// Make a single recv call, expecting a newline terminated request. Return the position of the newline, or -1 if no
// newline found (i.e. empty request or request that is too large)
ssize_t recv_request(int sockfd, char *buffer, size_t buffer_size);

#endif  // NETWORK_H

// vim: filetype=c :
