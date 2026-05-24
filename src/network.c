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

void send_data(int sockfd, const char *buffer, size_t buffer_size) {
  ssize_t num_bytes_sent;

  for (size_t buffer_pos = 0; buffer_pos < buffer_size; buffer_pos += num_bytes_sent) {
    num_bytes_sent = error_check_ssize_t(send(sockfd, buffer + buffer_pos, buffer_size - buffer_pos, 0));
  }
}

void send_file(int sockfd, int pagefd) {
  struct stat file_stat;
  error_check(fstat(pagefd, &file_stat));
  size_t file_size = file_stat.st_size;

  ssize_t num_bytes_sent;
  for (size_t total_bytes_sent = 0; total_bytes_sent < file_size; total_bytes_sent += num_bytes_sent) {
    num_bytes_sent = error_check_ssize_t(sendfile(sockfd, pagefd, NULL, file_size));
  }
}

ssize_t recv_data(int sockfd, char *buffer, size_t buffer_size) {
  ssize_t buff_offset = 0;

  while (buff_offset < (ssize_t)buffer_size) {
    ssize_t this_size = error_check_ssize_t(recv(sockfd, buffer + buff_offset, buffer_size - buff_offset, 0));
    buff_offset += this_size;

    if (this_size == 0) {
      return buff_offset;
    }
  }

  abort("Buffer of size %zu not large enough to hold message of size %zu", buffer_size, buff_offset);
}

ssize_t recv_request(int sockfd, char *buffer, size_t buffer_size) {
  ssize_t message_size = error_check_ssize_t(recv(sockfd, buffer, buffer_size, 0));
  String message_as_string = string_init((U8 *)buffer, message_size);

  U64 newline_pos = string_find_first(message_as_string, string_literal("\n"));

  if (newline_pos == U64NULL) {
    return -1;
  }

  return newline_pos;
}
