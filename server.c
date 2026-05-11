#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/definitions.h"
#include "base/string.h"
#include "helpers.h"

void web_server(void) {
  struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE};
  struct addrinfo *res;
  error_check(getaddrinfo(NULL, "3490", &hints, &res));

  int server_sockfd = error_check_int(socket(res->ai_family, res->ai_socktype, res->ai_protocol));

  error_check(bind(server_sockfd, res->ai_addr, res->ai_addrlen));
  error_check(listen(server_sockfd, 10));

  struct sockaddr_storage their_addr;
  socklen_t their_addr_size = sizeof(their_addr);

  int incoming_sockfd = error_check_int(accept(server_sockfd, (struct sockaddr *)&their_addr, &their_addr_size));
  printf("Accepted connection, sockfd %d\n", incoming_sockfd);

  char buffer[10000] = {0};
  recv(incoming_sockfd, buffer, sizeof(buffer), 0);

  String recv_str = string_init_cstring(buffer);
  Arena split_arena = arena_init(10000);
  StringArray split = string_split(&split_arena, recv_str, string_literal("\n"));

  printf("\n");
  for (U64 i = 0; i < split.count; ++i) {
    printf("%02" U64f ": %s\n", i, string_get_cstring(&split_arena, split.data[i]));
  }
  printf("\n");

  arena_free(&split_arena);

  const char *message = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello</h1><p>This is some smaller text</p>";
  send(incoming_sockfd, message, strlen(message), 0);
  printf("Sent message\n");

  close(incoming_sockfd);
  printf("Closed connection\n");

  close(server_sockfd);
  printf("Closed server\n");
}

int main(void) {
  web_server();

  return 0;
}
