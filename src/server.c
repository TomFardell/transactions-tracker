#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/data.h"
#include "base/date.h"
#include "base/definitions.h"
#include "base/string.h"
#include "constants.h"
#include "helpers.h"
#include "network.h"
#include "storage.h"
#include "transaction.h"

#define RECV_BUFFER_SIZE 1024

// Get whether a file can be accessed by a client
bool can_access_file(const String file_path) {
  // Don't allow requests that traverse out the directory
  if (string_contains(file_path, string_literal("../"))) {
    return false;
  }

  // Check if the file exists and is accessible
  Arena string_arena = arena_init(file_path.len + 1);
  int access_return = access(string_get_cstring(&string_arena, file_path), R_OK);
  arena_free(&string_arena);

  return (access_return != -1);
}

// Initialise and bind a server to a given port on this machine, returning its socket file descriptor
int server_init(const char *service) {
  struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE};
  struct addrinfo *info;
  error_check(getaddrinfo(NULL, service, &hints, &info));

  int sockfd = error_check_int(socket(info->ai_family, info->ai_socktype, info->ai_protocol));
  error_check(bind(sockfd, info->ai_addr, info->ai_addrlen));

  const int yes = 0;
  error_check(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  freeaddrinfo(info);

  return sockfd;
}

void send_404(int sockfd, const char *file_path) {
  int pagefd = error_check_int(open(file_path, O_RDONLY));
  printf("Opened file '%s' for sending\n", file_path);

  const char *message = "HTTP/1.1 404 Not Found\r\n\r\n";
  send_data(sockfd, message, strlen(message));
  send_file(sockfd, pagefd);
  printf("Sent 404 with file '%s'\n", file_path);

  error_check(close(pagefd));
  printf("Closed file '%s'\n", file_path);
}

void send_200(int sockfd, const char *file_path) {
  int pagefd = error_check_int(open(file_path, O_RDONLY));
  printf("Opened file '%s' for sending\n", file_path);

  const char *message = "HTTP/1.1 200 OK\r\n\r\n";
  send_data(sockfd, message, strlen(message));
  send_file(sockfd, pagefd);
  printf("Sent 200 with file '%s'\n", file_path);

  error_check(close(pagefd));
  printf("Closed file '%s'\n", file_path);
}

void handle_post(String request_body) {
  Arena post_arena = arena_init(request_body.len + 1);
  printf("Received POST request with body '%s'\n", string_get_cstring(&post_arena, request_body));
  arena_free(&post_arena);
}

// Given a socket file descriptor for an accepted incoming connection, receive and handle a single request
void handle_client(int in_sockfd) {
  char buffer[RECV_BUFFER_SIZE];

  ssize_t request_length = recv_request(in_sockfd, buffer, sizeof(buffer));
  if (request_length == 0) {
    printf("Ignoring empty request\n");
    return;
  }
  if (request_length == sizeof(buffer)) {
    printf("Ignoring request of length %zd (probably too large for buffer)\n", request_length);
    return;
  }

  Arena string_arena = arena_init(4 * RECV_BUFFER_SIZE);
  String recv_str = string_init(buffer, request_length);
  LinkNode *request_parts = string_split(&string_arena, recv_str, string_literal("\r\n\r\n"));
  String request_header = linked_list_get_container_node_at_index(request_parts, 0, StringNode, node)->data;
  LinkNode *request_header_lines = string_split(&string_arena, request_header, string_literal("\r\n"));
  String header_first_line = link_node_get_container_node(request_header_lines->next, StringNode, node)->data;

  LinkNode *header_words = string_split(&string_arena, header_first_line, string_literal(" "));
  if (linked_list_get_length(header_words) != 3) {
    printf("Ignoring request containing %" U64f " words\n", linked_list_get_length(header_words));

    arena_free(&string_arena);
    return;
  }

  String request_type = linked_list_get_container_node_at_index(header_words, 0, StringNode, node)->data;
  String request_arg = linked_list_get_container_node_at_index(header_words, 1, StringNode, node)->data;
  if (!string_equals(request_type, string_literal("GET")) &&
      !string_equals(request_type, string_literal("POST"))) {
    printf("Ignoring unexpected request of type '%s'\n", string_get_cstring(&string_arena, request_type));

    arena_free(&string_arena);
    return;
  }
  if (request_arg.str[0] != '/') {
    printf("Ignoring request with unexpected arg '%s'\n", string_get_cstring(&string_arena, request_arg));

    arena_free(&string_arena);
    return;
  }

  if (string_equals(request_type, string_literal("POST"))) {
    if (linked_list_get_length(request_parts) <= 1) {
      printf("Ignoring POST request with empty message body\n");

      arena_free(&string_arena);
      return;
    }
    String request_body = linked_list_get_container_node_at_index(request_parts, 1, StringNode, node)->data;
    handle_post(request_body);
  }

  String requested_file = string_init_substring(request_arg, 1, request_arg.len);
  requested_file = string_append(&string_arena, static_dir, requested_file);
  printf("Client made request for '%s' (maps to '%s')\n", string_get_cstring(&string_arena, request_arg),
         string_get_cstring(&string_arena, requested_file));

  if (!can_access_file(requested_file)) {
    printf("Unable to access file '%s'\n", string_get_cstring(&string_arena, requested_file));

    String not_found_path = string_append(&string_arena, static_dir, not_found_file);
    if (!can_access_file(not_found_path)) {
      abort("Unable to access Not Found file '%s'", string_get_cstring(&string_arena, not_found_path));
    }
    send_404(in_sockfd, string_get_cstring(&string_arena, not_found_path));

    arena_free(&string_arena);
    return;
  }

  send_200(in_sockfd, string_get_cstring(&string_arena, requested_file));
  arena_free(&string_arena);
  return;
}

void web_server(void) {
  int server_sockfd = server_init(PORT);
  U32 backlog_size = 10;
  error_check(listen(server_sockfd, backlog_size));
  printf("Listening for connections\n");
  printf("\n");

  while (true) {
    struct sockaddr_storage in_addr;
    socklen_t in_addr_size = sizeof(in_addr);

    int in_sockfd = error_check_int(accept(server_sockfd, (struct sockaddr *)&in_addr, &in_addr_size));
    if (in_addr.ss_family == AF_INET) {
      char ip_pres[INET_ADDRSTRLEN];
      inet_ntop(in_addr.ss_family, &(((struct sockaddr_in *)(&in_addr))->sin_addr), ip_pres, sizeof(ip_pres));
      printf("Accepted connection from IPv4 %s:%hu\n", ip_pres, ((struct sockaddr_in *)&in_addr)->sin_port);
    } else if (in_addr.ss_family == AF_INET6) {
      char ip_pres[INET6_ADDRSTRLEN];
      inet_ntop(in_addr.ss_family, &(((struct sockaddr_in6 *)(&in_addr))->sin6_addr), ip_pres, sizeof(ip_pres));
      printf("Accepted connection from IPv6 %s:%hu\n", ip_pres, ((struct sockaddr_in6 *)&in_addr)->sin6_port);
    } else {
      printf("Accepted unknown address\n");
    }

    handle_client(in_sockfd);

    error_check(close(in_sockfd));
    printf("Closed connection\n");
    printf("\n");
  }

  // I guess this is never hit :(
  error_check(close(server_sockfd));
  printf("Closed server\n");
}

int main(void) {
  web_server();

  return 0;
}
