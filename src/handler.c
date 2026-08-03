#include "handler.h"

#include <stdbool.h>
#include <stdio.h>

#include "base/definitions.h"
#include "base/string.h"

typedef struct LabelValuePair {
  String label;
  String value;
} LabelValuePair;

define_node(LabelValuePair);

// Parse the request body into a linked list of pairs of labels and values
static LinkNode *request_body_get_label_value_pairs(Arena *a, String request_body) {
  LinkNode *result = arena_alloc_single(a, LinkNode);
  linked_list_init(result);

  LinkNode *label_value_strings = string_split(a, request_body, string_literal("&"));
  foreach (pair, label_value_strings) {
    String pair_string = link_node_get_data(pair, String);

    LinkNode *label_and_value = string_split(a, pair_string, string_literal("="));
    if (linked_list_get_length(label_and_value) != 2) {
      continue;
    }

    String this_label = linked_list_get_data_at_index(label_and_value, 0, String);
    String this_value = linked_list_get_data_at_index(label_and_value, 1, String);

    LabelValuePairNode *this_pair_node = arena_alloc_single(a, LabelValuePairNode);
    this_pair_node->data = (LabelValuePair){this_label, this_value};

    linked_list_push_back(result, &this_pair_node->node);
  }

  return result;
}

// Given a parsed request body and a label, get strings for all values that follow that label
static LinkNode *label_value_pairs_get_values_from_label(Arena *a, LinkNode *label_value_pairs, String label) {
  LinkNode *result = arena_alloc_single(a, LinkNode);
  linked_list_init(result);

  foreach (pair_node, label_value_pairs) {
    LabelValuePair pair = link_node_get_data(pair_node, LabelValuePair);

    if (string_equals(pair.label, label)) {
      StringNode *value_string_node = arena_alloc_single(a, StringNode);
      value_string_node->data = pair.value;
      linked_list_push_back(result, &value_string_node->node);
    }
  }

  return result;
}

bool handle_post_data(MappingLists mapping_lists, String request_body) {
  Arena a = arena_init(2048);

  LinkNode *label_value_pairs = request_body_get_label_value_pairs(&a, request_body);

  LinkNode *instructions =
      label_value_pairs_get_values_from_label(&a, label_value_pairs, string_literal("*instruction"));
  if (linked_list_get_length(instructions) != 1) {
    printf("Got request with %" U64f " instructions\n", linked_list_get_length(instructions));
    goto return_false;
  }

  String instruction_string = linked_list_get_data_at_index(instructions, 0, String);

  LinkNode *instruction_arguments = string_split(&a, instruction_string, string_literal("-"));
  String instruction_name = linked_list_get_data_at_index(instruction_arguments, 0, String);
  linked_list_remove_at_index(instruction_arguments, 0);

  if (string_equals(instruction_name, string_literal("*add_to"))) {
    if (linked_list_get_length(instruction_arguments) != 1) {
      printf("Got '%" Stringf "' instruction with %" U64f " arguments\n", stringf_args(instruction_name),
             linked_list_get_length(instruction_arguments));
      goto return_false;
    }

    String list_name = linked_list_get_data_at_index(instruction_arguments, 0, String);
    ListMapping list_mapping = mapping_lists_locate_list_mapping(mapping_lists, list_name);
    if (list_mapping.item_internal_type == INTERNAL_TYPE_NULL) {
      printf("Couldn't locate list mapping '%" Stringf "'\n", stringf_args(list_name));
      goto return_false;
    }

    typedef struct PathInfo {
      U64 offset;
      String id_path;      // E.g. this.member1.member2
      String struct_path;  // E.g. type.member1.member2
      DisplayType display_type;
      InternalType internal_type;
    } PathInfo;

    define_node(PathInfo);

    LinkNode member_paths;
    linked_list_init(&member_paths);
    PathInfoNode initial_path = {.data = {.offset = 0,
                                          .id_path = string_literal("this"),
                                          .struct_path = list_mapping.item_struct_name,
                                          .display_type = list_mapping.item_display_type,
                                          .internal_type = list_mapping.item_internal_type}};
    linked_list_push_back(&member_paths, &initial_path.node);

    for (LinkNode *this_path = member_paths.next; this_path != &member_paths; /* Iteration inside body */) {
      PathInfo this_path_info = link_node_get_data(this_path, PathInfo);

      LinkNode *member_mappings_for_path =
          mapping_lists_get_member_mappings_for_struct(&a, mapping_lists, this_path_info.struct_path);

      // If we found member mappings then this struct has members. We should remove this struct from our list
      // and add the members instead
      if (linked_list_get_length(member_mappings_for_path) > 0) {
        // Add the members first
        foreach (this_member_node, member_mappings_for_path) {
          MemberMapping this_member_mapping = link_node_get_data(this_member_node, MemberMapping);
          PathInfoNode *this_path_info_node = arena_alloc_single(&a, PathInfoNode);
          this_path_info_node->data = (PathInfo){
              .offset = this_member_mapping.offset,
              .id_path =
                  string_concat(&a, 3, this_path_info.id_path, string_literal("."), this_member_mapping.name),
              .struct_path =
                  string_concat(&a, 3, this_path_info.struct_path, string_literal("."), this_member_mapping.name),
              .display_type = this_member_mapping.display_type,
              .internal_type = this_member_mapping.internal_type};

          linked_list_push_back(&member_paths, &this_path_info_node->node);
        }

        this_path = this_path->next;
        link_node_remove_from_linked_list(this_path->prev);
      } else {
        // No members so all there is to do is move the pointer forwards
        this_path = this_path->next;
      }
    }

    foreach (this_path_node, &member_paths) {
      PathInfo this_path_info = link_node_get_data(this_path_node, PathInfo);
      printf("-> %" Stringf "\n", stringf_args(this_path_info.id_path));
    }

    goto return_false;

  } else {
    printf("Got unrecognised instruction '%" Stringf "'\n", stringf_args(instruction_name));
    goto return_false;
  }

return_false:
  arena_free(&a);
  return false;
}
