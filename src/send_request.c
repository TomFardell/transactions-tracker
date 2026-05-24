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
  if (argc != 2) {
    printf("Please call with a single argument, the request to send\n");
    return EXIT_FAILURE;
  }

  // Add a newline to the command line argument
  size_t request_len = strlen(argv[1]);
  char *terminated_request = malloc((request_len + 2) * sizeof(*terminated_request));
  strcpy(terminated_request, argv[1]);
  terminated_request[request_len] = '\n';
  terminated_request[request_len + 1] = '\0';

  send_request(terminated_request);

  free(terminated_request);

  return 0;
}
