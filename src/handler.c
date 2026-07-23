#include "handler.h"

#include <stdbool.h>
#include <stdio.h>

#include "base/definitions.h"
#include "base/string.h"

// Given a request body and a label, get all values that follow that label, i.e. the body contains 'label=value'
static LinkNode *request_body_get_value_for_label(Arena *a, String request_body, String label) {
  LinkNode *result = arena_alloc_single(a, LinkNode);
  linked_list_init(result);

  LinkNode *label_value_pairs = string_split(a, request_body, string_literal("&"));
  for (LinkNode *pair = label_value_pairs->next; pair != label_value_pairs; pair = pair->next) {
    String pair_string = link_node_get_container_node(pair, StringNode, node)->data;
    LinkNode *label_and_value = string_split(a, pair_string, string_literal("="));
    if (linked_list_get_length(label_and_value) != 2) {
      continue;
    }

    String this_label = linked_list_get_container_node_at_index(label_and_value, 0, StringNode, node)->data;
    LinkNode *this_value_node = linked_list_get_node_at_index(label_and_value, 1);

    if (string_equals(this_label, label)) {
      // We allocated on the passed arena and are finished with the previous linked list so it is fine to reassign
      // this node like this (I think!)
      linked_list_push_back(result, this_value_node);
    }
  }

  return result;
}

// Given a request body, parse the instruction name. Returns an empty string if no instruction found
static String request_body_get_instruction(Arena *a, String request_body) {
  LinkNode *instructions_found = request_body_get_value_for_label(a, request_body, string_literal("*instruction"));
  if (linked_list_get_length(instructions_found) != 1) {
    return string_literal("");
  }

  return linked_list_get_container_node_at_index(instructions_found, 0, StringNode, node)->data;
}

bool handle_post_data(MappingLists mapping_lists, String request_body) {
  Arena a = arena_init(2048);

  String instruction_string = request_body_get_instruction(&a, request_body);
  if (string_equals(instruction_string, string_literal(""))) {
    printf("Got request with no instruction\n");
    goto return_false;
  }

  LinkNode *instruction_arguments = string_split(&a, instruction_string, string_literal("-"));
  String instruction_name =
      linked_list_get_container_node_at_index(instruction_arguments, 0, StringNode, node)->data;
  linked_list_remove_at_index(instruction_arguments, 0);

  if (string_equals(instruction_name, string_literal("*add_to"))) {
    if (linked_list_get_length(instruction_arguments) != 1) {
      printf("Got '%" Stringf "' instruction with %" U64f " arguments\n", stringf_args(instruction_name),
             linked_list_get_length(instruction_arguments));
      goto return_false;
    }

    String list_name = linked_list_get_container_node_at_index(instruction_arguments, 0, StringNode, node)->data;
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

    typedef struct PathInfoNode {
      PathInfo data;
      LinkNode node;
    } PathInfoNode;

    LinkNode member_paths;
    linked_list_init(&member_paths);
    PathInfoNode initial_path = {.data = {.offset = 0,
                                          .id_path = string_literal("this"),
                                          .struct_path = list_mapping.item_struct_name,
                                          .display_type = list_mapping.item_display_type,
                                          .internal_type = list_mapping.item_internal_type}};
    linked_list_push_back(&member_paths, &initial_path.node);

    for (LinkNode *this_path = member_paths.next; this_path != &member_paths; /* Iteration inside body */) {
      PathInfo this_path_info = link_node_get_container_node(this_path, PathInfoNode, node)->data;

      LinkNode *member_mappings_for_path =
          mapping_lists_get_member_mappings_for_struct(&a, mapping_lists, this_path_info.struct_path);

      // If we found member mappings then this struct has members. We should remove this struct from our list
      // and add the members instead
      if (linked_list_get_length(member_mappings_for_path) > 0) {
        // Add the members first
        for (LinkNode *this_member = member_mappings_for_path->next; this_member != member_mappings_for_path;
             this_member = this_member->next) {
          MemberMapping this_member_mapping =
              link_node_get_container_node(this_member, MemberMappingNode, node)->data;
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

    for (LinkNode *this_path = member_paths.next; this_path != &member_paths; this_path = this_path->next) {
      PathInfo this_path_info = link_node_get_container_node(this_path, PathInfoNode, node)->data;
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
