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
  Arena a = arena_init(2048);

  String instruction_string = request_body_get_instruction(request_body);
  LinkNode *instruction_list = string_split(&a, instruction_string, string_literal("-"));
  String instruction_name = linked_list_get_container_node_at_index(instruction_list, 0, StringNode, node)->data;
  LinkNode *instruction_arguments = instruction_list->next;

  if (string_equals(instruction_name, string_literal("*add_to"))) {
    if (linked_list_get_length(instruction_arguments) != 1) {
      printf("Got '%" Stringf "' instruction with %" U64f "arguments\n", stringf_args(instruction_name),
             linked_list_get_length(instruction_arguments));
      goto return_false;
    }

    String list_name = linked_list_get_container_node_at_index(instruction_arguments, 0, StringNode, node)->data;
    ListMapping list_mapping = mapping_lists_locate_list_mapping(mapping_lists, list_name);
    if (list_mapping.item_internal_type == INTERNAL_TYPE_NULL) {
      printf("Couldn't locate list mapping '%" Stringf "'\n", stringf_args(list_name));
      goto return_false;
    }
  } else {
    printf("Got unrecognised instruction '%" Stringf "'\n", stringf_args(instruction_name));
    goto return_false;
  }

return_false:
  arena_free(&a);
  return false;
}
