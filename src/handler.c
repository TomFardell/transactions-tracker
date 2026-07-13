#include "handler.h"

#include <stdbool.h>
#include <stdio.h>

#include "base/definitions.h"
#include "base/string.h"
#include "constants.h"

// Given a request body, parse the instruction name. Returns an empty string if no instruction found
static String request_body_get_instruction(String request_body) {
  U64 instruction_label_pos = string_find_first(request_body, instruction_label);
  if (instruction_label_pos == U64NULL) {
    return string_literal("");
  }

  U64 after_instruction_label_pos = instruction_label_pos + instruction_label.len;
  if (after_instruction_label_pos > request_body.len || request_body.str[after_instruction_label_pos] != '=') {
    return string_literal("");
  }

  String after_instruction_label =
      string_init_substring(request_body, after_instruction_label_pos + 1, request_body.len);

  U64 instruction_name_end = string_find_first(after_instruction_label, string_literal("&"));
  if (instruction_name_end == U64NULL) {
    return after_instruction_label;
  }

  return string_init_substring(after_instruction_label, 0, instruction_name_end);
}

bool handle_post_data(MappingLists mapping_lists, String request_body) {
  String instruction = request_body_get_instruction(request_body);
  printf("Instruction: %" Stringf "\n", stringf_args(instruction));

  return false;
}
