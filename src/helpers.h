#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "base/definitions.h"

#define abort(...) statement(_abort(__FILE__, __LINE__, __func__, __VA_ARGS__))
noreturn void _abort(const char *file, int line, const char *func, ...);

#define error_check(status) statement(_error_check(__FILE__, __LINE__, __func__, status))
void _error_check(const char *file, int line, const char *func, int status);

#define error_check_int(result) _error_check_int(__FILE__, __LINE__, __func__, result)
int _error_check_int(const char *file, int line, const char *func, int result);

#define error_check_ssize_t(result) _error_check_ssize_t(__FILE__, __LINE__, __func__, result)
ssize_t _error_check_ssize_t(const char *file, int line, const char *func, ssize_t result);

#define error_check_ptr(result) _error_check_ptr(__FILE__, __LINE__, __func__, result)
void *_error_check_ptr(const char *file, int line, const char *func, void *result);

#define error_check_fread_fwrite(result, check_file) \
  _error_check_fread_fwrite(__FILE__, __LINE__, __func__, result, check_file)
size_t _error_check_fread_fwrite(const char *file, int line, const char *func, size_t result, FILE *check_file);

#endif  // HELPERS_H

// vim: filetype=c :
