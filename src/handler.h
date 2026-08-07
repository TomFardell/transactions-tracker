#ifndef HANDLER_H
#define HANDLER_H

#include <stdbool.h>

#include "base/memory.h"
#include "base/string.h"
#include "parser.h"

// Given a POST request, parse it and (if possible) make the requested updates to the data described by the passed
// mapping lists. Returns whether the request was valid and the updates were made
bool handle_post_data(Arena *mappings_arena, MappingLists mapping_lists, String request_body);

#endif  // HANDLER_H

// vim: filetype=c :
