#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/definitions.h"
#include "base/string.h"
#include "helpers.h"

#define RECV_BUFFER_SIZE 10000

const String static_path = string_literal("static/");
const String not_found_file = string_literal("error.html");
const char *port = "3490";

// Get whether a file can be accessed by a client
bool can_access_file(String file_path) {
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

// Make repeated recv calls to read incoming data on a socket into a buffer, returning the number of bytes read
ssize_t recv_data(int sockfd, char *buffer, size_t buffer_size) {
  ssize_t buffer_pos = 0;

  while (buffer_pos < (ssize_t)buffer_size) {
    buffer_pos += error_check_ssize_t(recv(sockfd, buffer + buffer_pos, buffer_size - buffer_pos, 0));

    if (buffer_pos == 0) {
      printf("Received empty message\n");
      return 0;
    }
    if (buffer_pos < 4) {
      abort("Recieved message of invalid size %zu", buffer_pos);
    }

    String terminator = string_init((U8 *)(buffer + buffer_pos) - 4, 4);
    if (string_equals(terminator, string_literal("\r\n\r\n"))) {
      return buffer_pos;
    }
  }

  abort("Buffer of size %zu not large enough to hold message of size %zu", buffer_size, buffer_pos);
}

// Make repeated send calls to send an entire buffer of data on a given socket
void send_data(int sockfd, const char *buffer, size_t buffer_size) {
  size_t buffer_pos = 0;
  ssize_t num_bytes_sent;

  for (; buffer_pos < buffer_size; buffer_pos += num_bytes_sent) {
    num_bytes_sent = error_check_ssize_t(send(sockfd, buffer + buffer_pos, buffer_size - buffer_pos, 0));
  }
}

// Make repeated sendfile calls to send a file on a given socket
void send_file(int sockfd, int fd, size_t file_size) {
  size_t total_bytes_sent = 0;
  ssize_t num_bytes_sent;

  for (; total_bytes_sent < file_size; total_bytes_sent += num_bytes_sent) {
    num_bytes_sent = error_check_ssize_t(sendfile(sockfd, fd, NULL, file_size));
  }
}

void send_page(int sockfd, int pagefd) {
  struct stat file_stat;
  error_check(fstat(pagefd, &file_stat));
  off_t page_size = file_stat.st_size;

  send_file(sockfd, pagefd, page_size);
}

void send_404(int sockfd, const char *file_path) {
  int pagefd = error_check_int(open(file_path, O_RDONLY));

  const char *message = "HTTP/1.1 404 Not Found\r\n\r\n";
  send_data(sockfd, message, strlen(message));
  send_page(sockfd, pagefd);
  printf("Sent 404 with file '%s'\n", file_path);

  error_check(close(pagefd));
  printf("Closed file\n");
}

void send_200(int sockfd, const char *file_path) {
  int pagefd = error_check_int(open(file_path, O_RDONLY));

  const char *message = "HTTP/1.1 200 OK\r\n\r\n";
  send_data(sockfd, message, strlen(message));
  send_page(sockfd, pagefd);
  printf("Sent 200 with file '%s'\n", file_path);

  error_check(close(pagefd));
  printf("Closed file\n");
}

// Given a socket file descriptor for an accepted incoming connection, serve the requested pages
void handle_client(int in_sockfd) {
  char buffer[RECV_BUFFER_SIZE];

  // If we received nothing, do nothing
  if (recv_data(in_sockfd, buffer, sizeof(buffer)) == 0) {
    return;
  }

  U64 arena_size = 2000;
  Arena string_arena = arena_init(arena_size);

  String recv_str = string_init_cstring(buffer);
  LinkNode *lines = string_split(&string_arena, recv_str, string_literal("\n"));

  String request_line = link_node_get_container_node(lines->next, StringNode, node)->data;
  LinkNode *request_words = string_split(&string_arena, request_line, string_literal(" "));
  if (linked_list_get_length(request_words) != 3) {
    printf("Unexpected request containing %" U64f " words\n", linked_list_get_length(request_words));

    arena_free(&string_arena);
    return;
  }

  String request_type = linked_list_get_container_node_at_index(request_words, 0, StringNode, node)->data;
  String request_arg = linked_list_get_container_node_at_index(request_words, 1, StringNode, node)->data;
  if (!string_equals(request_type, string_literal("GET"))) {
    printf("Unexpected request type '%s'\n", string_get_cstring(&string_arena, request_type));

    arena_free(&string_arena);
    return;
  }
  if (request_arg.str[0] != '/') {
    printf("Unexpected character in path '%s'\n", string_get_cstring(&string_arena, request_arg));

    arena_free(&string_arena);
    return;
  }

  String requested_file = string_init_substring(request_arg, 1, request_arg.len);
  requested_file = string_append(&string_arena, static_path, requested_file);
  printf("Client requested file '%s'\n", string_get_cstring(&string_arena, requested_file));

  if (!can_access_file(requested_file)) {
    printf("Unable to access file '%s'\n", string_get_cstring(&string_arena, requested_file));
    String not_found_path = string_append(&string_arena, static_path, not_found_file);
    if (!can_access_file(not_found_path)) {
      abort("Unable to access Not Found file '%s'", string_get_cstring(&string_arena, not_found_path));
    }
    send_404(in_sockfd, string_get_cstring(&string_arena, not_found_path));

    arena_free(&string_arena);
    return;
  }

  send_200(in_sockfd, string_get_cstring(&string_arena, requested_file));

  arena_free(&string_arena);
}

void web_server(void) {
  int server_sockfd = server_init(port);
  U32 backlog_size = 10;
  error_check(listen(server_sockfd, backlog_size));
  printf("Listening for connections\n");

  while (true) {
    struct sockaddr_in in_addr;
    socklen_t in_addr_size = sizeof(in_addr);

    int in_sockfd = error_check_int(accept(server_sockfd, (struct sockaddr *)&in_addr, &in_addr_size));

    handle_client(in_sockfd);

    error_check(close(in_sockfd));
    printf("Closed connection\n");
  }

  // I guess this is never hit :(
  error_check(close(server_sockfd));
  printf("Closed server\n");
}

int main(void) {
  web_server();

  return 0;
}
