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
#include "base/definitions.h"
#include "base/string.h"
#include "constants.h"
#include "handler.h"
#include "helpers.h"
#include "network.h"
#include "parser.h"
#include "storage.h"

#define RECV_BUFFER_SIZE 2048

// Get whether a file can be accessed by the client
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

  // Allow restarting the server without needing to wait for the kernel to free that socket
  const int yes = 1;
  error_check(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  error_check(bind(sockfd, info->ai_addr, info->ai_addrlen));

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

void handle_get(Arena *string_arena, String request_arg, MappingLists mapping_lists, int sockfd) {
  if (request_arg.str[0] != '/') {
    printf("Ignoring request with unexpected arg '%" Stringf "'\n", stringf_args(request_arg));

    return;
  }

  String requested_file = string_init_substring(request_arg, 1, request_arg.len);  // Remove the '/'

  // Figure out the filetype (everything after the first '.') of the requested file
  String requested_file_type = string_literal("");
  U64 requested_file_dot_pos = string_find_first(requested_file, string_literal("."));
  if (requested_file_dot_pos != U64NULL) {
    requested_file_type = string_init_substring(requested_file, requested_file_dot_pos + 1, requested_file.len);
  }

  String static_path = string_append(string_arena, static_dir, requested_file);
  // Don't parse non-html files
  String parsed_path = string_equals(requested_file_type, string_literal("html"))
                           ? string_append(string_arena, parsed_dir, requested_file)
                           : static_path;

  printf("Client made request for '%" Stringf "' (maps to '%" Stringf "')\n", stringf_args(request_arg),
         stringf_args(parsed_path));

  // If the client requests a file they shouldn't, send a 404 (and do this before any parsing)
  if (!can_access_file(static_path)) {
    printf("Unable to access file '%" Stringf "'\n", stringf_args(static_path));

    String not_found_path = string_append(string_arena, static_dir, not_found_file);
    if (!can_access_file(not_found_path)) {
      abort("Unable to access Not Found file '% " Stringf "'", stringf_args(not_found_path));
    }
    send_404(sockfd, string_get_cstring(string_arena, not_found_path));

    return;
  }

  if (string_equals(requested_file_type, string_literal("html"))) {
    parse_file_into(static_path, parsed_path, mapping_lists);
    printf("Parsed file '%" Stringf "' into '%" Stringf "')\n", stringf_args(static_path),
           stringf_args(parsed_path));
  }

  send_200(sockfd, string_get_cstring(string_arena, parsed_path));
}

void handle_post(Arena *a, const LinkNode *request_header_body, MappingLists mapping_lists) {
  if (linked_list_get_length(request_header_body) <= 1) {
    printf("Ignoring POST request with no message body\n");
    return;
  }

  String request_body = linked_list_get_data_at_index(request_header_body, 1, String);
  printf("Received POST request with body:\n%" Stringf "\n", stringf_args(request_body));

  if (handle_post_data(a, mapping_lists, request_body)) {
    printf("POST request handled successfully\n");
  } else {
    printf("POST request not handled\n");
  }
}

// Given a socket file descriptor for an accepted incoming connection, receive and handle a single request
void handle_client(Arena *mappings_arena, int in_sockfd, MappingLists mapping_lists) {
  char buffer[RECV_BUFFER_SIZE];

  ssize_t bytes_received = recv_request(in_sockfd, buffer, sizeof(buffer));
  if (bytes_received == 0) {
    printf("Ignoring empty request\n");
    return;
  }
  if (bytes_received == RECV_BUFFER_SIZE) {
    printf("Ignoring request of length %zd (probably too large for buffer)\n", bytes_received);
    return;
  }

  Arena string_arena = arena_init(4 * RECV_BUFFER_SIZE);

  LinkNode *request_header_body =
      string_split(&string_arena, string_init(buffer, bytes_received), string_literal("\r\n\r\n"));

  String request_header = linked_list_get_data_at_index(request_header_body, 0, String);
  LinkNode *request_header_lines = string_split(&string_arena, request_header, string_literal("\r\n"));

  String header_first_line = linked_list_get_data_at_index(request_header_lines, 0, String);
  LinkNode *header_first_line_words = string_split(&string_arena, header_first_line, string_literal(" "));

  if (linked_list_get_length(header_first_line_words) != 3) {
    printf("Ignoring request containing %" U64f " words\n", linked_list_get_length(header_first_line_words));

    arena_free(&string_arena);
    return;
  }

  String request_type = linked_list_get_data_at_index(header_first_line_words, 0, String);
  String request_arg = linked_list_get_data_at_index(header_first_line_words, 1, String);

  if (string_equals(request_type, string_literal("GET"))) {
    handle_get(&string_arena, request_arg, mapping_lists, in_sockfd);
  } else if (string_equals(request_type, string_literal("POST"))) {
    handle_post(mappings_arena, request_header_body, mapping_lists);
    handle_get(&string_arena, request_arg, mapping_lists, in_sockfd);  // Also send the requested file
  } else {
    printf("Ignoring unexpected request of type '%" Stringf "'\n", stringf_args(request_type));
  }

  arena_free(&string_arena);
}

void web_server(void) {
  // Worryingly this arena getting filled up will crash the program. Uhhhh will fix later
  Arena mappings_arena = arena_init(16384);
  LinkNode *transactions = retrieve_transactions(&mappings_arena, string_literal("test.dat"));
  U32 num_add_transaction_inputs = 4;  // Placeholder
  MappingInput mapping_input = {.transactions = transactions,
                                .num_add_transaction_inputs = &num_add_transaction_inputs};
  const MappingLists mapping_lists = mapping_lists_init(&mappings_arena, mapping_input);

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

    handle_client(&mappings_arena, in_sockfd, mapping_lists);

    error_check(close(in_sockfd));
    printf("Closed connection\n");
    printf("\n");
  }

  // I guess this is never hit :(
  arena_free(&mappings_arena);
  error_check(close(server_sockfd));
  printf("Closed server\n");
}

int main(void) {
  web_server();

  return 0;
}
