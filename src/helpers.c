#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

noreturn void _abort(const char *file, int line, const char *func, ...) {
  int errno_start = errno;

  va_list args;
  va_start(args, func);

  const char *message = va_arg(args, const char *);

  fprintf(stderr, "\n");
  fprintf(stderr, "> ---| Error |---\n");
  fprintf(stderr, "> Error in %s->%s (line %d)\n", file, func, line);
  fprintf(stderr, "> ");
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  if (errno_start != 0) {
    fprintf(stderr, ">\n");
    fprintf(stderr, "> Error %d: %s\n", errno_start, strerror(errno_start));
  }

  fprintf(stderr, "Terminating program\n");

  va_end(args);

  exit(EXIT_FAILURE);
}

void _error_check(const char *file, int line, const char *func, int status) {
  if (status == -1) {
    _abort(file, line, func, "Error check failed");
  }
}

int _error_check_int(const char *file, int line, const char *func, int result) {
  if (result == -1) {
    _abort(file, line, func, "Error check failed");
  }

  return result;
}

ssize_t _error_check_ssize_t(const char *file, int line, const char *func, ssize_t result) {
  if (result == -1) {
    _abort(file, line, func, "Error check failed");
  }

  return result;
}

void *_error_check_ptr(const char *file, int line, const char *func, void *result) {
  if (result == NULL) {
    _abort(file, line, func, "Error check failed");
  }

  return result;
}
