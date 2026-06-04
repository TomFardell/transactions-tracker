#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "constants.h"
#include "helpers.h"
#include "network.h"

#define RECV_BUFFER_SIZE 10000

void send_request(const char *request) {
  struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE};
  struct addrinfo *res;
  error_check(getaddrinfo(NULL, PORT, &hints, &res));

  int server_sockfd = error_check_int(socket(res->ai_family, res->ai_socktype, res->ai_protocol));

  error_check(connect(server_sockfd, res->ai_addr, res->ai_addrlen));
  send_data(server_sockfd, request, strlen(request) + 1);

  printf("---| Sent |---\n");
  printf("%s\n", request);

  char buffer[RECV_BUFFER_SIZE];
  recv_data(server_sockfd, buffer, RECV_BUFFER_SIZE);

  printf("---| Received |---\n");
  printf("%s\n", buffer);

  close(server_sockfd);
}

int main(int argc, char **argv) {
  if (argc < 2 || 3 < argc) {
    fprintf(stderr, "Call with either a request header or both a request header and body.\n");
    return EXIT_FAILURE;
  }

  size_t header_len = strlen(argv[1]);
  size_t request_len = header_len + 3;  // One \r\n and a \0
  if (argc == 3) {
    request_len += 2 + strlen(argv[2]);  // Additional \r\n
  }
  char *formatted_request = malloc(request_len);

  strcpy(formatted_request, argv[1]);
  formatted_request[header_len] = '\r';
  formatted_request[header_len + 1] = '\n';
  if (argc == 3) {
    formatted_request[header_len + 2] = '\r';
    formatted_request[header_len + 3] = '\n';
    strcpy(formatted_request + header_len + 4, argv[2]);
  }
  formatted_request[request_len] = '\0';

  send_request(formatted_request);

  free(formatted_request);

  return 0;
}
