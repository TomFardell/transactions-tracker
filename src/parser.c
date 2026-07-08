#include "parser.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/compound_types.h"
#include "base/date.h"
#include "base/definitions.h"
#include "base/memory.h"
#include "base/string.h"
#include "constants.h"
#include "helpers.h"
#include "stdio.h"
#include "sys/stat.h"
#include "transaction.h"

MappingLists mapping_lists_init(Arena *a, MappingInput mapping_input) {
  MappingLists result = {arena_alloc_single(a, LinkNode), arena_alloc_single(a, LinkNode),
                         arena_alloc_single(a, LinkNode)};
  linked_list_init(result.list_mappings);
  linked_list_init(result.item_mappings);
  linked_list_init(result.member_mappings);

  ListMappingNode *list_mapping_node;

  list_mapping_node = arena_alloc_single(a, ListMappingNode);
  list_mapping_node->data.items = mapping_input.transactions;
  list_mapping_node->data.name = string_literal("transactions");
  list_mapping_node->data.item_struct_name = string_literal("Transaction");
  list_mapping_node->data.item_display_type = DISPLAY_TYPE_NONE;
  list_mapping_node->data.item_internal_type = INTERNAL_TYPE_TRANSACTION;
  linked_list_push_back(result.list_mappings, &list_mapping_node->node);

  ItemMappingNode *item_mapping_node;
  item_mapping_node = arena_alloc_single(a, ItemMappingNode);
  item_mapping_node->data.item = mapping_input.num_add_transaction_inputs;
  item_mapping_node->data.name = string_literal("num_add_transaction_inputs");
  item_mapping_node->data.struct_name = string_literal("U32");
  item_mapping_node->data.display_type = DISPLAY_TYPE_NUMBER;
  item_mapping_node->data.internal_type = INTERNAL_TYPE_U32;
  linked_list_push_back(result.item_mappings, &item_mapping_node->node);

  MemberMappingNode *member_mapping_node;

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, desc);
  member_mapping_node->data.name = string_literal("desc");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_TEXT;
  member_mapping_node->data.internal_type = INTERNAL_TYPE_STRING;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, date);
  member_mapping_node->data.name = string_literal("date");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_DATE;
  member_mapping_node->data.internal_type = INTERNAL_TYPE_DATE;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, amount);
  member_mapping_node->data.name = string_literal("amount");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_CURRENCY;
  member_mapping_node->data.internal_type = INTERNAL_TYPE_F32;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  return result;
}

ListMapping mapping_lists_locate_list_mapping(MappingLists mapping_lists, String list_mapping_name) {
  for (LinkNode *p = mapping_lists.list_mappings->next; p != mapping_lists.list_mappings; p = p->next) {
    ListMapping this_list_mapping = link_node_get_container_node(p, ListMappingNode, node)->data;

    if (string_equals(this_list_mapping.name, list_mapping_name)) {
      return this_list_mapping;
    }
  }
  abort("Unable to find list mapping of name '%" Stringf "'", stringf_args(list_mapping_name));
}

ItemMapping mapping_lists_locate_item_mapping(MappingLists mapping_lists, String item_mapping_name) {
  for (LinkNode *p = mapping_lists.item_mappings->next; p != mapping_lists.item_mappings; p = p->next) {
    ItemMapping this_item_mapping = link_node_get_container_node(p, ItemMappingNode, node)->data;

    if (string_equals(this_item_mapping.name, item_mapping_name)) {
      return this_item_mapping;
    }
  }
  abort("Unable to find item mapping of name '%" Stringf "'", stringf_args(item_mapping_name));
}

MemberMapping mapping_lists_locate_member_mapping(MappingLists mapping_lists, String member_mapping_struct_name,
                                                  String member_mapping_name) {
  for (LinkNode *mapping_node = mapping_lists.member_mappings->next; mapping_node != mapping_lists.member_mappings;
       mapping_node = mapping_node->next) {
    MemberMapping this_member_mapping = link_node_get_container_node(mapping_node, MemberMappingNode, node)->data;

    if (string_equals(this_member_mapping.name, member_mapping_name) &&
        string_equals(this_member_mapping.struct_name, member_mapping_struct_name)) {
      return this_member_mapping;
    }
  }
  abort("Unable to find member mapping of name '%" Stringf "' and type '%" Stringf "'",
        stringf_args(member_mapping_name), stringf_args(member_mapping_struct_name));
}

// Return type of mapping_lists_get_member_info_from_path
typedef struct MemberInfo {
  U64 offset;
  DisplayType display_type;
  InternalType internal_type;
} MemberInfo;

// Given mapping lists and a member path (i.e. {"BaseType", "member1", "member2"}), calculate its offset (from base
// type) and its display type
static MemberInfo mapping_lists_get_member_info_from_path(MappingLists mapping_lists, LinkNode *member_path) {
  U64 member_depth = linked_list_get_length(member_path);
  if (member_depth <= 1) {
    abort("Cannot get member from path of length '%" U64f "'", member_depth);
  }

  Arena a = arena_init(512);

  U64 member_offset = 0;
  DisplayType member_display_type;
  InternalType member_internal_type;
  String member_path_so_far = linked_list_get_container_node_at_index(member_path, 0, StringNode, node)->data;

  for (LinkNode *p = linked_list_get_node_at_index(member_path, 1); p != member_path; p = p->next) {
    String member_name = link_node_get_container_node(p, StringNode, node)->data;

    MemberMapping member_mapping =
        mapping_lists_locate_member_mapping(mapping_lists, member_path_so_far, member_name);

    member_offset += member_mapping.offset;
    member_display_type = member_mapping.display_type;
    member_internal_type = member_mapping.internal_type;
    member_path_so_far = string_concat(&a, 3, member_path_so_far, string_literal("."), member_mapping.struct_name);
  }

  arena_free(&a);
  return (MemberInfo){member_offset, member_display_type, member_internal_type};
}

// Return type of mapping_lists_get_var_info_from_item_name
typedef struct VarInfo {
  void *item;
  DisplayType display_type;
  InternalType internal_type;
} VarInfo;

// Given mapping lists and an identifier (i.e. "my_var.member1.member2"), extract the required info to use the
// underlying data that is mapped to
static VarInfo mapping_lists_get_var_info_from_item_name(MappingLists mapping_lists, String item_name,
                                                         ItemMapping this_mapping) {
  Arena a = arena_init(512);
  LinkNode *member_path = string_split(&a, item_name, string_literal("."));
  String base_item_name = linked_list_get_container_node_at_index(member_path, 0, StringNode, node)->data;

  ItemMapping item_mapping;
  if (string_equals(base_item_name, string_literal("this"))) {
    item_mapping = this_mapping;
  } else {
    item_mapping = mapping_lists_locate_item_mapping(mapping_lists, item_name);
  }

  void *item = item_mapping.item;
  DisplayType display_type = item_mapping.display_type;
  InternalType internal_type = item_mapping.internal_type;

  // If there are members, search the member mappings
  if (linked_list_get_length(member_path) > 1) {
    // Replace the item in the member path with its struct name
    StringNode *base_type_name_node = arena_alloc_single(&a, StringNode);
    base_type_name_node->data = item_mapping.struct_name;
    linked_list_remove_at_index(member_path, 0);
    linked_list_push_front(member_path, &base_type_name_node->node);

    MemberInfo member_info = mapping_lists_get_member_info_from_path(mapping_lists, member_path);
    item = (U8 *)item + member_info.offset;
    display_type = member_info.display_type;
    internal_type = member_info.internal_type;
  }

  return (VarInfo){item, display_type, internal_type};
}

// Find the start position of the closing tag of the passed command. Specifically handles nested commands of the
// same type
static U64 find_closing_tag_pos(String data, String tag_name) {
  I32 tag_depth = 0;
  U64 cursor_pos = 0;
  String remaining_data = data;

  while (true) {
    U64 next_tag_opener_pos = string_find_first(remaining_data, parse_tag_opener);
    U64 next_tag_closer_pos = string_find_first(remaining_data, parse_tag_closer);
    if (next_tag_opener_pos == U64NULL) {
      return U64NULL;
    }

    char command_type = data.str[cursor_pos + next_tag_opener_pos + parse_tag_opener.len];
    String tag_contents = string_init_substring(data, cursor_pos + next_tag_opener_pos + parse_tag_opener.len + 1,
                                                cursor_pos + next_tag_closer_pos);
    U64 first_space_pos = string_find_first(tag_contents, string_literal(" "));
    String command_name =
        (first_space_pos == U64NULL) ? tag_contents : string_init_substring(tag_contents, 0, first_space_pos);

    if ((command_type == '/' || command_type == '#') && string_equals(command_name, tag_name)) {
      tag_depth += (command_type == '#') - (command_type == '/');

      if (tag_depth == 0) {
        return cursor_pos + next_tag_opener_pos;
      }
    }

    cursor_pos += next_tag_closer_pos + parse_tag_closer.len + 1;
    remaining_data = string_init_substring(data, cursor_pos, data.len);
  }
}

// Get a pointer to the data in the container node of a link node where the container node is associated with a
// given internal type
static void *internal_type_get_data_from_item_node(const LinkNode *item_node, InternalType internal_type) {
  switch (internal_type) {
    case INTERNAL_TYPE_NULL: {
      abort("Cannot process list with null internal types");
    }
    case INTERNAL_TYPE_DATE: {
      abort("Date nodes currently not implemented");
    }
    case INTERNAL_TYPE_F32: {
      return &link_node_get_container_node(item_node, F32Node, node)->data;
    }
    case INTERNAL_TYPE_F64: {
      return &link_node_get_container_node(item_node, F64Node, node)->data;
    }
    case INTERNAL_TYPE_STRING: {
      return &link_node_get_container_node(item_node, StringNode, node)->data;
    }
    case INTERNAL_TYPE_TRANSACTION: {
      return &link_node_get_container_node(item_node, TransactionNode, node)->data;
    }
    case INTERNAL_TYPE_U32: {
      return &link_node_get_container_node(item_node, U32Node, node)->data;
    }
    default: {
      abort("Unrecognised internal type '%d'", (int)internal_type);
    }
  };
}

// Given a pointer to some memory and internal and display types, get the string that should be displayed
static String types_get_string_output(Arena *a, const void *item, InternalType internal_type,
                                      DisplayType display_type) {
  switch (display_type) {
    case DISPLAY_TYPE_CURRENCY: {
      switch (internal_type) {
        case (INTERNAL_TYPE_F32): {
          return string_format(a, "£%.02" F32f, *((F32 *)item));
        }
        case (INTERNAL_TYPE_F64): {
          return string_format(a, "£%.02" F64f, *((F64 *)item));
        }
        case (INTERNAL_TYPE_U32): {
          return string_format(a, "£%.02" U32f, *((U32 *)item));
        }

        default: {
          abort("Cannot display item of internal type '%d' as currency", (int)internal_type);
        }
      }
    }

    case DISPLAY_TYPE_DATE: {
      switch (internal_type) {
        case (INTERNAL_TYPE_DATE): {
          return date_get_string(a, *((Date *)item), date_format, day_of_week_format);
        }

        default: {
          abort("Cannot display item of internal type '%d' as date", (int)internal_type);
        }
      }
    }

    case DISPLAY_TYPE_NUMBER: {
      switch (internal_type) {
        case (INTERNAL_TYPE_F32): {
          return string_format(a, "%.02" F32f, *((F32 *)item));
        }
        case (INTERNAL_TYPE_F64): {
          return string_format(a, "%.02" F64f, *((F64 *)item));
        }
        case (INTERNAL_TYPE_U32): {
          return string_format(a, "%" U32f, *((U32 *)item));
        }

        default: {
          abort("Cannot display item of internal type '%d' as integer", (int)internal_type);
        }
      }
    }

    case DISPLAY_TYPE_TEXT: {
      switch (internal_type) {
        case (INTERNAL_TYPE_STRING): {
          return *((String *)item);
        }

        default: {
          abort("Cannot display item of internal type '%d' as text", (int)internal_type);
        }
      }
    }

    default: {
      abort("Cannot display item with display type '%d'", (int)display_type);
    }
  }
}

// Given a pointer to some data and its internal type, cast the value of the data to an F64
F64 internal_type_cast_to_F64(const void *item, InternalType internal_type) {
  switch (internal_type) {
    case INTERNAL_TYPE_DATE: {
      return (F64)(*((Date *)item));
    }
    case INTERNAL_TYPE_F32: {
      return (F64)(*((F32 *)item));
    }
    case INTERNAL_TYPE_F64: {
      return *((F64 *)item);
    }
    case INTERNAL_TYPE_U32: {
      return (F64)(*((U32 *)item));
    }
    default: {
      abort("Cannot convert item of internal type '%" U64f "' to F64", internal_type);
    }
  }
}

// Recursively called function that parses the next tag in its input, constructing the result in the passed
// string builder. Also takes info on what the keyword 'this' refers to
static void _parse_string(Arena *a, String data, MappingLists mapping_lists, ItemMapping this_mapping,
                          LinkNode *sb) {
  U64 next_tag_opener_pos = string_find_first(data, parse_tag_opener);
  U64 next_tag_closer_pos = string_find_first(data, parse_tag_closer);

  if (next_tag_opener_pos == U64NULL && next_tag_closer_pos == U64NULL) {
    string_builder_add_string(a, sb, data);
    return;
  }
  if (next_tag_opener_pos == U64NULL || next_tag_closer_pos == U64NULL) {
    abort("Different number of openers ('%" Stringf "') and closers ('%" Stringf "')",
          stringf_args(parse_tag_opener), stringf_args(parse_tag_closer));
  }

  U64 cursor_pos = 0;

  // Add everything up to the found tag
  string_builder_add_string(a, sb, string_init_substring(data, cursor_pos, next_tag_opener_pos));

  // Get the stuff inside the tag and split it into words
  String tag_contents =
      string_init_substring(data, next_tag_opener_pos + parse_tag_opener.len, next_tag_closer_pos);
  LinkNode *tag_words = string_split(a, tag_contents, string_literal(" "));

  if (tag_contents.str[0] == '#') {
    /*------------*/
    /* # commands */
    /*---------------------------------------------------------------------------------------------------------*/
    String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;

    if (string_equals(command, string_literal("#each"))) {
      /*-------*/
      /* #each */
      /*-------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 2) {
        abort("Got %" U64f " arguments in '#each' command tag (expected 2)", linked_list_get_length(tag_words));
      }
      String argument = linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data;

      U64 closing_tag_opener_pos = find_closing_tag_pos(data, string_literal("each"));
      if (closing_tag_opener_pos == U64NULL) {
        abort("No closing '/each' tag for '#each' command");
      }

      String data_between_tags =
          string_init_substring(data, next_tag_closer_pos + parse_tag_closer.len, closing_tag_opener_pos);

      // Search for a matching list mapping
      ListMapping list_mapping = mapping_lists_locate_list_mapping(mapping_lists, argument);

      // Loop through the actual items
      for (LinkNode *item_node = list_mapping.items->next; item_node != list_mapping.items;
           item_node = item_node->next) {
        ItemMapping this_item_mapping = {
            .item = internal_type_get_data_from_item_node(item_node, list_mapping.item_internal_type),
            .name = string_literal("this"),  // Not really needed
            .struct_name = list_mapping.item_struct_name,
            .display_type = list_mapping.item_display_type,
            .internal_type = list_mapping.item_internal_type,
        };

        _parse_string(a, data_between_tags, mapping_lists, this_item_mapping, sb);
      }

      // Place the cursor after the closing /each tag
      cursor_pos =
          closing_tag_opener_pos +
          string_find_first(string_init_substring(data, closing_tag_opener_pos, data.len), parse_tag_closer) +
          parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else if (string_equals(command, string_literal("#sum"))) {
      /*------*/
      /* #sum */
      /*-------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 2) {
        abort("Got %" U64f " arguments in '#sum' command tag (expected 2)", linked_list_get_length(tag_words));
      }
      String argument = linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data;

      LinkNode *member_path = string_split(a, argument, string_literal("."));
      String list_name = linked_list_get_container_node_at_index(member_path, 0, StringNode, node)->data;

      // Search for a matching list mapping
      ListMapping list_mapping = mapping_lists_locate_list_mapping(mapping_lists, list_name);

      // Replace the list name with its struct name and get the offset and display type of the member
      StringNode *base_type_name_node = arena_alloc_single(a, StringNode);
      base_type_name_node->data = list_mapping.item_struct_name;
      linked_list_remove_at_index(member_path, 0);
      linked_list_push_front(member_path, &base_type_name_node->node);

      MemberInfo member_info = mapping_lists_get_member_info_from_path(mapping_lists, member_path);

      union {
        F32 currency_f32;
        F64 currency_f64;
        U32 currency_u32;
        F32 number_f32;
        F64 number_f64;
        U32 number_u32;
        String text_string;
      } result = {0};

      // Now loop through each item and sum the result
      for (LinkNode *item_node = list_mapping.items->next; item_node != list_mapping.items;
           item_node = item_node->next) {
        void *item = (U8 *)internal_type_get_data_from_item_node(item_node, list_mapping.item_internal_type) +
                     member_info.offset;

        switch (member_info.display_type) {
          case (DISPLAY_TYPE_CURRENCY): {
            switch (member_info.internal_type) {
              case INTERNAL_TYPE_F32: {
                result.currency_f32 += *((F32 *)item);
                break;
              }
              case INTERNAL_TYPE_F64: {
                result.currency_f64 += *((F64 *)item);
                break;
              }
              case INTERNAL_TYPE_U32: {
                result.currency_u32 += *((U32 *)item);
                break;
              }
              default: {
                abort("Unable to sum the list '%" Stringf "' of item internal types '%d' as a currency",
                      stringf_args(argument), (int)list_mapping.item_internal_type);
              }
            }
            break;
          }
          // Obviously this is the exact same as currency, but maybe in the future we'd want to sum this slightly
          // differently, hence the union gets a bunch more elements!
          case (DISPLAY_TYPE_NUMBER): {
            switch (member_info.internal_type) {
              case INTERNAL_TYPE_F32: {
                result.number_f32 += *((F32 *)item);
                break;
              }
              case INTERNAL_TYPE_F64: {
                result.number_f64 += *((F64 *)item);
                break;
              }
              case INTERNAL_TYPE_U32: {
                result.number_u32 += *((U32 *)item);
                break;
              }
              default: {
                abort("Unable to sum the list '%" Stringf "' of item internal types '%d' as a number",
                      stringf_args(argument), (int)list_mapping.item_internal_type);
              }
            }
            break;
          }

          // This is stupid and I'll probably never use it, but we can sum (concatenate) strings
          case (DISPLAY_TYPE_TEXT): {
            switch (member_info.internal_type) {
              case INTERNAL_TYPE_STRING: {
                // If we are in the first iteration, there is nothing to append to
                if (item_node == list_mapping.items->next) {
                  result.text_string = *((String *)item);
                } else {
                  result.text_string = string_append(a, result.text_string, *((String *)item));
                }
                break;
              }
              // Maybe I'll eventually allow concatenation of numeric types as strings (i.e. 1 + 1 == 11)
              default: {
                abort("Unable to sum the list '%" Stringf "' of item internal types '%d' as a string",
                      stringf_args(argument), (int)list_mapping.item_internal_type);
              }
            }
            break;
          }

          default: {
            abort("'%" Stringf "' has unsummable display type '%d'", stringf_args(argument),
                  (int)member_info.display_type);
          }
        }
      }

      String item_output =
          types_get_string_output(a, &result, member_info.internal_type, member_info.display_type);
      string_builder_add_string(a, sb, item_output);

      cursor_pos = next_tag_closer_pos + parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else if (string_equals(command, string_literal("#if"))) {
      /*-----*/
      /* #if */
      /*-------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 4) {
        abort("Got %" U64f " arguments in '#each' command tag (expected 4)", linked_list_get_length(tag_words));
      }
      String identifiers[2] = {linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data,
                               linked_list_get_container_node_at_index(tag_words, 3, StringNode, node)->data};
      String operator = linked_list_get_container_node_at_index(tag_words, 2, StringNode, node)->data;

      // These F64s are only used if their respective identifiers are literals. In the code below we are going to
      // just do all the comparisons with all data casted to F64s. I suppose there are cases where we might get
      // incorrect comparisons if we lose precision due to using F64s, i.e. if we are comparing a huge U64 to a
      // huge literal. However, using F64s for all literals here should suffice for any normal use
      F64 values[2];

      for (I32 i = 0; i < 2; ++i) {
        // Anything beginning with a number, a '.' or a '-' cannot be an identifier, so treat as a literal
        char first_char = identifiers[i].str[0];
        if (('0' <= first_char && first_char <= '9') || first_char == '.' || first_char == '-') {
          error_check(sscanf(string_get_cstring(a, identifiers[i]), "%" F64f, values + i));
        } else {
          VarInfo var_info =
              mapping_lists_get_var_info_from_item_name(mapping_lists, identifiers[i], this_mapping);
          values[i] = internal_type_cast_to_F64(var_info.item, var_info.internal_type);
        }
      }

      bool if_tag_evaluation;

      if (string_equals(operator, string_literal("=="))) {
        if_tag_evaluation = F64_eq(values[0], values[1]);
      } else if (string_equals(operator, string_literal("<"))) {
        if_tag_evaluation = F64_lt(values[0], values[1]);
      } else if (string_equals(operator, string_literal("<="))) {
        if_tag_evaluation = F64_leq(values[0], values[1]);
      } else if (string_equals(operator, string_literal(">"))) {
        if_tag_evaluation = F64_gt(values[0], values[1]);
      } else if (string_equals(operator, string_literal(">="))) {
        if_tag_evaluation = F64_geq(values[0], values[1]);
      } else if (string_equals(operator, string_literal("!="))) {
        if_tag_evaluation = F64_neq(values[0], values[1]);
      } else {
        abort("Unknown comparison operator '% " Stringf "' in '#if' command", stringf_args(operator));
      }

      U64 closing_tag_opener_pos = find_closing_tag_pos(data, string_literal("if"));
      if (closing_tag_opener_pos == U64NULL) {
        abort("No closing '/if' tag for '#if' command");
      }

      String data_between_tags =
          string_init_substring(data, next_tag_closer_pos + parse_tag_closer.len, closing_tag_opener_pos);

      // Only recurse on the contents of the #if block if its condition evaluated to true
      if (if_tag_evaluation) {
        _parse_string(a, data_between_tags, mapping_lists, this_mapping, sb);
      }

      // Place the cursor after the closing /if tag
      cursor_pos =
          closing_tag_opener_pos +
          string_find_first(string_init_substring(data, closing_tag_opener_pos, data.len), parse_tag_closer) +
          parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else if (string_equals(command, string_literal("#iter"))) {
      /*-------*/
      /* #iter */
      /*-------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 2) {
        abort("Got %" U64f " arguments in '#iter' command tag (expected 2)", linked_list_get_length(tag_words));
      }
      String argument = linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data;

      U64 closing_tag_opener_pos = find_closing_tag_pos(data, string_literal("iter"));
      if (closing_tag_opener_pos == U64NULL) {
        abort("No closing '/iter' tag for '#iter' command");
      }

      String data_between_tags =
          string_init_substring(data, next_tag_closer_pos + parse_tag_closer.len, closing_tag_opener_pos);

      VarInfo var_info = mapping_lists_get_var_info_from_item_name(mapping_lists, argument, this_mapping);

      if (var_info.internal_type != INTERNAL_TYPE_U32) {
      }

      U64 iter_count;
      switch (var_info.internal_type) {
        case (INTERNAL_TYPE_U32): {
          iter_count = *((U32 *)var_info.item);
          break;
        }
        default: {
          abort("Cannot iterate over item of internal type '%d'", (int)var_info.internal_type);
        }
      }

      U64 *i = arena_alloc_single(a, U64);
      ItemMapping this_item_mapping = {
          .item = i,
          .name = string_literal("this"),  // Not really needed
          .struct_name = string_literal("U64"),
          .display_type = var_info.display_type,
          .internal_type = var_info.internal_type,
      };

      // Iterate. Can then use 'this' to retreive the iteration index
      for (*i = 0; *i < iter_count; ++(*i)) {
        _parse_string(a, data_between_tags, mapping_lists, this_item_mapping, sb);
      }

      // Place the cursor after the closing /iter tag
      cursor_pos =
          closing_tag_opener_pos +
          string_find_first(string_init_substring(data, closing_tag_opener_pos, data.len), parse_tag_closer) +
          parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else {
      abort("Unrecognised command '#%" Stringf "'", stringf_args(command));
    }
    /*---------------------------------------------------------------------------------------------------------*/
  } else if (tag_contents.str[0] == '/') {
    /*------------*/
    /* / commands */
    /*---------------------------------------------------------------------------------------------------------*/
    if (linked_list_get_length(tag_words) != 1) {
      abort("Got %" U64f " arguments in command termination tag (expected 1)", linked_list_get_length(tag_words));
    }
    String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;

    if (string_equals(command, string_literal("/each"))) {
      /*-------*/
      /* /each */
      /*-------------------------------------------------------------------------------------------------------*/
      // Do nothing but move the cursor

      cursor_pos = next_tag_closer_pos + parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else if (string_equals(command, string_literal("/if"))) {
      /*-----*/
      /* /if */
      /*-------------------------------------------------------------------------------------------------------*/
      // Do nothing but move the cursor

      cursor_pos = next_tag_closer_pos + parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else if (string_equals(command, string_literal("/iter"))) {
      /*-----*/
      /* /if */
      /*-------------------------------------------------------------------------------------------------------*/
      // Do nothing but move the cursor

      cursor_pos = next_tag_closer_pos + parse_tag_closer.len;
      /*-------------------------------------------------------------------------------------------------------*/
    } else {
      abort("Unrecognised command termination '/% " Stringf "'", stringf_args(command));
    }
    /*---------------------------------------------------------------------------------------------------------*/
  } else {
    /*------------*/
    /* identifier */
    /*---------------------------------------------------------------------------------------------------------*/
    if (linked_list_get_length(tag_words) != 1) {
      abort("Got %" U64f " arguments in identifier tag (expected 1)", linked_list_get_length(tag_words));
    }

    String item_name = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;

    VarInfo var_info = mapping_lists_get_var_info_from_item_name(mapping_lists, item_name, this_mapping);
    String item_output = types_get_string_output(a, var_info.item, var_info.internal_type, var_info.display_type);
    string_builder_add_string(a, sb, item_output);

    cursor_pos = next_tag_closer_pos + parse_tag_closer.len;
    /*---------------------------------------------------------------------------------------------------------*/
  }

  // Call again with the remainder of the data
  _parse_string(a, string_init_substring(data, cursor_pos, data.len), mapping_lists, this_mapping, sb);
}

String parse_string(Arena *a, String data, const MappingLists mapping_lists) {
  LinkNode sb;
  linked_list_init(&sb);

  _parse_string(a, data, mapping_lists, (ItemMapping){0}, &sb);

  return string_builder_get_string(a, &sb);
}

void parse_file_into(String in_path, String out_path, const MappingLists mapping_lists) {
  Arena parse_arena = arena_init(8192);

  int infd = error_check_int(open(string_get_cstring(&parse_arena, in_path), O_RDONLY));
  struct stat file_stat;
  error_check(fstat(infd, &file_stat));
  U64 file_size = file_stat.st_size;
  char *in_data = arena_alloc_array(&parse_arena, char, file_size);
  error_check_ssize_t(read(infd, in_data, file_size));
  error_check(close(infd));

  String out_data = parse_string(&parse_arena, string_init(in_data, file_size), mapping_lists);

  FILE *out_file = error_check_ptr(fopen(string_get_cstring(&parse_arena, out_path), "w"));
  error_check_fread_fwrite(fwrite(out_data.str, 1, out_data.len, out_file), out_file);
  error_check(fclose(out_file));

  arena_free(&parse_arena);
}
