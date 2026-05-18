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

#define BUFFER_SIZE 10000

const char *port = "3490";
const U64 general_arena_size = 10000;
const U32 backlog_size = 10;

// Initialise and bind a server to a given port on this machine
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

void web_server(void) {
  char buffer[BUFFER_SIZE];
  Arena general_arena = arena_init(general_arena_size);

  int server_sockfd = server_init(port);
  error_check(listen(server_sockfd, backlog_size));
  printf("Listening for connections\n");

  while (true) {
    struct sockaddr_in in_addr;
    socklen_t in_addr_size = sizeof(in_addr);

    int in_sockfd = error_check_int(accept(server_sockfd, (struct sockaddr *)&in_addr, &in_addr_size));

    if (recv_data(in_sockfd, buffer, sizeof(buffer)) == 0) {
      goto cleanup_incoming;
    }

    String recv_str = string_init_cstring(buffer);
    LinkNode *lines_split_head = string_split(&general_arena, recv_str, string_literal("\n"));

    String request_line = link_node_get_container_node(lines_split_head->next, StringNode, node)->data;
    LinkNode *words_split_head = string_split(&general_arena, request_line, string_literal(" "));
    if (linked_list_get_length(words_split_head) != 3) {
      printf("Unexpected request containing %" U64f " words\n", linked_list_get_length(words_split_head));
      goto cleanup_incoming;
    }

    String request_type = linked_list_get_container_node_at_index(words_split_head, 0, StringNode, node)->data;
    String request_arg = linked_list_get_container_node_at_index(words_split_head, 1, StringNode, node)->data;
    // TODO: split again on '&'
    if (!string_equals(request_type, string_literal("GET"))) {
      printf("Unexpected request type '%s'\n", string_get_cstring(&general_arena, request_type));
      goto cleanup_incoming;
    }
    if (request_arg.str[0] != '/') {
      printf("Unexpected character in path '%s'\n", string_get_cstring(&general_arena, request_arg));
      goto cleanup_incoming;
    }

    String request_path = string_init_substring(request_arg, 1, request_arg.len);
    printf("Requested file '%s'\n", string_get_cstring(&general_arena, request_path));

    int page_fd = open(string_get_cstring(&general_arena, request_path), O_RDONLY);
    if (page_fd == -1) {
      printf("Unable to find file '%s'\n", string_get_cstring(&general_arena, request_path));
      const char *message = "HTTP/1.1 404 Not Found\r\n\r\n";
      send_data(in_sockfd, message, strlen(message));
      printf("Sent 404\n");

      goto cleanup_incoming;
    }

    struct stat file_stat;
    error_check(fstat(page_fd, &file_stat));
    off_t page_size = file_stat.st_size;

    const char *message = "HTTP/1.1 200 OK\r\n\r\n";
    send_data(in_sockfd, message, strlen(message));
    send_file(in_sockfd, page_fd, page_size);
    printf("Sent 200 with file '%s'\n", string_get_cstring(&general_arena, request_path));

  cleanup_incoming:
    if (page_fd != -1) {
      error_check(close(page_fd));
      printf("Closed file\n");
    }

    error_check(close(in_sockfd));
    printf("Closed connection\n");
  }

  error_check(close(server_sockfd));
  printf("Closed server\n");

  arena_free(&general_arena);
}

int main(void) {
  web_server();

  return 0;
}
