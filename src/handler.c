#include "handler.h"

#include <stdbool.h>
#include <stdio.h>

#include "base/compound_types.h"
#include "base/data.h"
#include "base/date.h"
#include "base/definitions.h"
#include "base/memory.h"
#include "base/string.h"
#include "helpers.h"
#include "parser.h"
#include "transaction.h"

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

    String this_label = linked_list_get_data_at_index(label_and_value, 0, String);
    String this_value = (linked_list_get_length(label_and_value) == 2)
                            ? linked_list_get_data_at_index(label_and_value, 1, String)
                            : string_literal("");

    LabelValuePairNode *this_pair_node = arena_alloc_single(a, LabelValuePairNode);
    this_pair_node->data = (LabelValuePair){this_label, this_value};

    linked_list_push_back(result, &this_pair_node->node);
  }

  return result;
}

// Given a parsed request body and a label, count the number of values for that label
U64 label_value_pairs_count_values_for_label(const LinkNode *label_value_pairs, String label) {
  U64 total = 0;
  foreach (pair_node, label_value_pairs) {
    LabelValuePair pair = link_node_get_data(pair_node, LabelValuePair);

    if (string_equals(pair.label, label)) {
      ++total;
    }
  }

  return total;
}

// Remove the first instruction label-value pair, returning the value
String label_value_pairs_pop_instruction_value(LinkNode *label_value_pairs) {
  foreach (pair_node, label_value_pairs) {
    LabelValuePair pair = link_node_get_data(pair_node, LabelValuePair);
    if (string_equals(pair.label, string_literal("*instruction"))) {
      link_node_remove_from_linked_list(pair_node);
      return pair.value;
    }
  }

  return string_literal("");
}

// Given a pointer to some memory, a simple internal type and the string value, parse the value to
// create an allocated instance of the type at the location
static bool parse_into_location_simple(Arena *mappings_arena, void *location, InternalType internal_type,
                                       String value) {
  Arena string_arena = arena_init(value.len + 1);
  const char *data_cstring = string_get_cstring(&string_arena, value);

  switch (internal_type) {
    case INTERNAL_TYPE_DATE: {
      U32 day, month, year;
      if (sscanf(data_cstring, "%" U32f "-%" U32f "-%" U32f, &year, &month, &day) != 3) {
        printf("Failed to parse date '%s'\n", data_cstring);
        return false;
      }

      // Should probably not be lazy and write a date checking function in the base layer. Maybe we even want a
      // very generic date parsing method that can handle any format? That would be lovely but I can't be bothered
      // so here we are
      // TODO: Add to base layer at least
      if ((year < MIN_YEAR || MAX_YEAR < year) || (month < MONTH_JAN || MONTH_DEC < month) ||
          (day < 1 || month_get_days_in_month(month) < day) ||
          (day == 29 && month == 2 && !year_is_leap_year(year))) {
        printf("Got invalid date %" U32f "-%" U32f "-%" U32f "\n", year, month, day);
        return false;
      }

      *((Date *)location) = date_init(day, month, year);
      return true;
    }
    case (INTERNAL_TYPE_F32): {
      F32 result;
      if (sscanf(data_cstring, "%" F32f, &result) != 1) {
        printf("Failed to parse F32 '%s'\n", data_cstring);
        return false;
      }

      *((F32 *)location) = result;
      return true;
    }
    case (INTERNAL_TYPE_F64): {
      F64 result;
      if (sscanf(data_cstring, "%" F64f, &result) != 1) {
        printf("Failed to parse F64 '%s'\n", data_cstring);
        return false;
      }

      *((F64 *)location) = result;
      return true;
    }
    case (INTERNAL_TYPE_STRING): {
      // We copy here to ensure the string's underlying data has the correct lifetime
      String result = string_copy(mappings_arena, value);

      *((String *)location) = result;
      return true;
    }
    case (INTERNAL_TYPE_U32): {
      U32 result;
      if (sscanf(data_cstring, "%" U32f, &result) != 1) {
        printf("Failed to parse U32 '%s'\n", data_cstring);
        return false;
      }

      *((U32 *)location) = result;
      return true;
    }
    default: {
      printf("Cannot parse data with internal type '%d'. This is not a simple type", (int)internal_type);
      return false;
    }
  }
}

// Given a pointer to some memory, a compound internal type and the relevant label-value pairs, parse the values to
// create an allocated instance of the type at the location
static bool parse_into_location_compound(Arena *mappings_arena, void *location, InternalType internal_type,
                                         const LinkNode *label_value_pairs) {
  // At this point, we can assume we have the correct labels in the passed list
  switch (internal_type) {
    case INTERNAL_TYPE_TRANSACTION: {
      bool success = true;
      foreach (label_value_pair_node, label_value_pairs) {
        LabelValuePair label_value_pair = link_node_get_data(label_value_pair_node, LabelValuePair);
        String label = string_init_substring(label_value_pair.label,
                                             string_find_first(label_value_pair.label, string_literal(".")) + 1,
                                             label_value_pair.label.len);

        // Unfortunately this has to be hard coded
        if (string_equals(label, string_literal("desc"))) {
          if (!parse_into_location_simple(mappings_arena, &((Transaction *)location)->desc, INTERNAL_TYPE_STRING,
                                          label_value_pair.value)) {
            success = false;
          };
        } else if (string_equals(label, string_literal("date"))) {
          if (!parse_into_location_simple(mappings_arena, &((Transaction *)location)->date, INTERNAL_TYPE_DATE,
                                          label_value_pair.value)) {
            success = false;
          };
        } else if (string_equals(label, string_literal("amount"))) {
          if (!parse_into_location_simple(mappings_arena, &((Transaction *)location)->amount, INTERNAL_TYPE_F32,
                                          label_value_pair.value)) {
            success = false;
          };
        } else {
          abort("Passed label '%" Stringf "' with Transaction to compound parse function", stringf_args(label));
        }
      }

      return success;
    }
    default: {
      printf("Cannot parse data with internal type '%d'. This is not a compound type", (int)internal_type);
      return false;
    }
  }
}

// Given a list mapping and a linked list of the relevant label-value pairs, parse the values for each label and
// add the resulting element to the list mapping
static bool add_to_list_mapping(Arena *mappings_arena, ListMapping list_mapping,
                                const LinkNode *label_value_pairs) {
  LinkNode *new_node;
  switch (list_mapping.item_internal_type) {
    case INTERNAL_TYPE_DATE: {
      DateNode *this_node = arena_alloc_single(mappings_arena, DateNode);

      String value = linked_list_get_data_at_index(label_value_pairs, 0, LabelValuePair).value;
      if (!parse_into_location_simple(mappings_arena, &this_node->data, list_mapping.item_internal_type, value)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    case INTERNAL_TYPE_F32: {
      F32Node *this_node = arena_alloc_single(mappings_arena, F32Node);

      String value = linked_list_get_data_at_index(label_value_pairs, 0, LabelValuePair).value;
      if (!parse_into_location_simple(mappings_arena, &this_node->data, list_mapping.item_internal_type, value)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    case INTERNAL_TYPE_F64: {
      F64Node *this_node = arena_alloc_single(mappings_arena, F64Node);

      String value = linked_list_get_data_at_index(label_value_pairs, 0, LabelValuePair).value;
      if (!parse_into_location_simple(mappings_arena, &this_node->data, list_mapping.item_internal_type, value)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    case INTERNAL_TYPE_STRING: {
      StringNode *this_node = arena_alloc_single(mappings_arena, StringNode);

      String value = linked_list_get_data_at_index(label_value_pairs, 0, LabelValuePair).value;
      if (!parse_into_location_simple(mappings_arena, &this_node->data, list_mapping.item_internal_type, value)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    case INTERNAL_TYPE_TRANSACTION: {
      TransactionNode *this_node = arena_alloc_single(mappings_arena, TransactionNode);

      if (!parse_into_location_compound(mappings_arena, &this_node->data, list_mapping.item_internal_type,
                                        label_value_pairs)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    case INTERNAL_TYPE_U32: {
      U32Node *this_node = arena_alloc_single(mappings_arena, U32Node);

      String value = linked_list_get_data_at_index(label_value_pairs, 0, LabelValuePair).value;
      if (!parse_into_location_simple(mappings_arena, &this_node->data, list_mapping.item_internal_type, value)) {
        return false;
      }
      new_node = &this_node->node;
      break;
    }
    default: {
      abort("Cannot add to list whose elements have internal type 'NULL'");
      return false;
    }
  }

  linked_list_push_back(list_mapping.items, new_node);
  return true;
}

bool handle_post_data(Arena *mappings_arena, MappingLists mapping_lists, String request_body) {
  Arena handler_arena = arena_init(4096);

  LinkNode *label_value_pairs = request_body_get_label_value_pairs(&handler_arena, request_body);

  // TODO: Somehow figure out how to allow longer requests uhhhh
  U64 num_instructions =
      label_value_pairs_count_values_for_label(label_value_pairs, string_literal("*instruction"));
  if (num_instructions != 1) {
    printf("Got request with %" U64f " instructions\n", num_instructions);
    goto return_false;
  }

  String instruction_string = label_value_pairs_pop_instruction_value(label_value_pairs);

  LinkNode *instruction_arguments = string_split(&handler_arena, instruction_string, string_literal("-"));
  String instruction_name = linked_list_get_data_at_index(instruction_arguments, 0, String);
  linked_list_remove_at_index(instruction_arguments, 0);

  if (string_equals(instruction_name, string_literal("*add_to"))) {
    /*---------*/
    /* *add_to */
    /*---------------------------------------------------------------------------------------------------------*/
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

    // Construct an array containing path infos for each descendant member of the list mapping's struct
    for (LinkNode *this_path = member_paths.next; this_path != &member_paths; /* Iteration inside body */) {
      PathInfo this_path_info = link_node_get_data(this_path, PathInfo);

      LinkNode *member_mappings_for_path =
          mapping_lists_get_member_mappings_for_struct(&handler_arena, mapping_lists, this_path_info.struct_path);

      // If we found member mappings then this struct has members. We should remove this struct from our list
      // and add the members instead
      if (linked_list_get_length(member_mappings_for_path) > 0) {
        // Add the members first
        foreach (this_member_node, member_mappings_for_path) {
          MemberMapping this_member_mapping = link_node_get_data(this_member_node, MemberMapping);
          PathInfoNode *this_path_info_node = arena_alloc_single(&handler_arena, PathInfoNode);
          this_path_info_node->data =
              (PathInfo){.offset = this_member_mapping.offset,
                         .id_path = string_concat(&handler_arena, 3, this_path_info.id_path, string_literal("."),
                                                  this_member_mapping.name),
                         .struct_path = string_concat(&handler_arena, 3, this_path_info.struct_path,
                                                      string_literal("."), this_member_mapping.name),
                         .display_type = this_member_mapping.display_type,
                         .internal_type = this_member_mapping.internal_type};

          linked_list_push_back(&member_paths, &this_path_info_node->node);
        }

        this_path = this_path->next;
        link_node_remove_from_linked_list(this_path->prev);
      } else {
        // No members so all there is to do is move to the next element in the linked list
        this_path = this_path->next;
      }
    }

    U64 group_size = linked_list_get_length(&member_paths);
    U64 num_values_in_body = linked_list_get_length(label_value_pairs);
    if (num_values_in_body % group_size != 0) {
      printf("Cannot split %" U64f " request values into groups of %" U64f "\n", num_values_in_body, group_size);
      goto return_false;
    }

    // Group the label-value pairs from the request into groups where each group (should) contain the data required
    // to construct an element of the mapped list. Note, that even if one group fails (e.g. if a value cannot be
    // parsed), groups that do not fail will result in their respective elements being added to the mapped list
    bool all_groups_successful = true;
    for (U64 group_num = 0; group_num < num_values_in_body / group_size; ++group_num) {
      LinkNode group_pairs;
      linked_list_init(&group_pairs);

      // Copy the label-value pairs of this group into the new linked list
      for (U64 value_num = 0; value_num < group_size; ++value_num) {
        LabelValuePairNode *this_pair_node = arena_alloc_single(&handler_arena, LabelValuePairNode);
        this_pair_node->data =
            linked_list_get_data_at_index(label_value_pairs, value_num + group_size * group_num, LabelValuePair);
        linked_list_push_back(&group_pairs, &this_pair_node->node);
      }

      // Now check that this group contains a label for each of the expected members
      bool group_is_complete = true;
      foreach (member_path_node, &member_paths) {
        PathInfo member_path_info = link_node_get_data(member_path_node, PathInfo);

        bool found_label = false;
        foreach (label_value_pair_node, &group_pairs) {
          LabelValuePair label_value_pair = link_node_get_data(label_value_pair_node, LabelValuePair);

          if (string_equals(member_path_info.id_path, label_value_pair.label)) {
            found_label = true;
            break;
          }
        }

        if (!found_label) {
          printf("No label in request body for member '%" Stringf "' in group %" U64f "\n",
                 stringf_args(member_path_info.id_path), group_num);
          group_is_complete = false;
          break;
        }
      }

      if (!group_is_complete) {
        all_groups_successful = false;
        continue;
      }

      printf("Attempting to add an element to list mapping '%" Stringf "' using the following data:\n",
             stringf_args(list_name));
      foreach (label_value_pair_node, &group_pairs) {
        LabelValuePair label_value_pair = link_node_get_data(label_value_pair_node, LabelValuePair);
        printf("> %" Stringf " = %" Stringf "\n", stringf_args(label_value_pair.label),
               stringf_args(label_value_pair.value));
      }

      if (add_to_list_mapping(mappings_arena, list_mapping, &group_pairs)) {
        printf("Added group %" U64f " of labels and values to list mapping '%" Stringf "\n", group_num,
               stringf_args(list_name));
      } else {
        printf("Unable add group %" U64f " of labels and values to list mapping '%" Stringf "\n", group_num,
               stringf_args(list_name));
        all_groups_successful = false;
      }
    }

    arena_free(&handler_arena);
    return all_groups_successful;

    /*---------------------------------------------------------------------------------------------------------*/
  } else {
    printf("Got unrecognised instruction '%" Stringf "'\n", stringf_args(instruction_name));
    goto return_false;
  }

return_false:
  arena_free(&handler_arena);
  return false;
}
