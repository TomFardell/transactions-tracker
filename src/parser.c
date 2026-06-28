#include "parser.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/compound_types.h"
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
  list_mapping_node->data.item_node_offset = offset_of(TransactionNode, node);
  linked_list_push_back(result.list_mappings, &list_mapping_node->node);

  MemberMappingNode *member_mapping_node;

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, desc);
  member_mapping_node->data.name = string_literal("desc");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_TEXT;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, date);
  member_mapping_node->data.name = string_literal("date");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_DATE;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.offset = offset_of(Transaction, amount);
  member_mapping_node->data.name = string_literal("amount");
  member_mapping_node->data.struct_name = string_literal("Transaction");
  member_mapping_node->data.display_type = DISPLAY_TYPE_CURRENCY;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  return result;
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

// Find the start position of the closing tag of the passed command. Can handle nested commands
static U64 find_closing_tag(String data, String tag_name, LinkNode *current_open_node, LinkNode *opening_tags) {
  I32 tag_depth = 1;
  for (LinkNode *open_node = current_open_node->next; open_node != opening_tags; open_node = open_node->next) {
    U64 open_pos = link_node_get_container_node(open_node, U64Node, node)->data;
    char command_type = data.str[open_pos + parse_tag_opener.len];

    if (command_type != '/' && command_type != '#') {
      continue;
    }
    if (!string_equals(string_init(data.str + open_pos + parse_tag_opener.len + 1, tag_name.len), tag_name)) {
      continue;
    }

    tag_depth += (command_type == '#') - (command_type == '/');

    if (tag_depth == 0) {
      return open_pos;
    }
  }

  return U64NULL;
}

// Parse a given string but with info on which item mapping the keyword "this" refers to
static String _parse_string(Arena *a, String data, MappingLists mapping_lists, const ItemMapping this_mapping) {
  LinkNode *opening_tag_positions = string_find_all(a, data, parse_tag_opener);
  LinkNode *closing_tag_positions = string_find_all(a, data, parse_tag_closer);

  if (linked_list_get_length(opening_tag_positions) != linked_list_get_length(closing_tag_positions)) {
    abort("Different number of '%" Stringf "'s (%" U64f ") and '%" Stringf "'s (% " U64f ")",
          stringf_args(parse_tag_opener), linked_list_get_length(opening_tag_positions),
          stringf_args(parse_tag_closer), linked_list_get_length(closing_tag_positions));
  }

  // If there is nothing to parse, don't do anything
  if (linked_list_get_length(opening_tag_positions) == 0) {
    return data;
  }

  LinkNode sb;
  linked_list_init(&sb);
  U64 cursor_pos = 0;

  for (LinkNode *open_node = opening_tag_positions->next, *close_node = closing_tag_positions->next;
       open_node != opening_tag_positions; open_node = open_node->next, close_node = close_node->next) {
    U64 open_pos = link_node_get_container_node(open_node, U64Node, node)->data;
    U64 close_pos = link_node_get_container_node(close_node, U64Node, node)->data;

    string_builder_add_string(a, &sb, string_init_substring(data, cursor_pos, open_pos));
    String tag_contents = string_init_substring(data, open_pos + parse_tag_opener.len, close_pos);

    LinkNode *tag_words = string_split(a, tag_contents, string_literal(" "));
    if (tag_contents.str[0] == '#') {
      /*------------*/
      /* # commands */
      /*---------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 2) {
        abort("Got %" U64f " arguments in command tag (expected 2)", linked_list_get_length(tag_words));
      }
      String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;
      String argument = linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data;

      if (string_equals(command, string_literal("#each"))) {
        /*-------*/
        /* #each */
        /*-------------------------------------------------------------------------------------------------------*/
        U64 closing_tag_opener_pos =
            find_closing_tag(data, string_literal("each"), open_node, opening_tag_positions);
        String data_between_tags =
            string_init_substring(data, close_pos + parse_tag_closer.len, closing_tag_opener_pos);

        // Search for a matching list mapping
        for (LinkNode *p = mapping_lists.list_mappings->next; p != mapping_lists.list_mappings; p = p->next) {
          ListMapping this_list_mapping = link_node_get_container_node(p, ListMappingNode, node)->data;

          if (!string_equals(this_list_mapping.name, argument)) {
            if (p->next == mapping_lists.list_mappings) {
              abort("No mapping found for #each argument '%" Stringf "'", stringf_args(argument));
            }
            continue;
          }

          // Loop through the actual items
          for (LinkNode *item_node = this_list_mapping.items->next; item_node != this_list_mapping.items;
               item_node = item_node->next) {
            // Yeah not gonna lie, this code is complete garbage
            ItemMapping this_item_mapping = {
                .item = (U8 *)item_node - this_list_mapping.item_node_offset,  // Manually get container node
                .name = string_literal("this"),                                // Not really needed
                .struct_name = this_list_mapping.item_struct_name,             // Need this if getting members
                .display_type = this_list_mapping.item_display_type};          // Need this if displaying the items

            String parsed_result = _parse_string(a, data_between_tags, mapping_lists, this_item_mapping);

            string_builder_add_string(a, &sb, parsed_result);
          }
        }

        // Make a new recursive call on everything after the #each block
        U64 closing_tag_closer_pos =
            closing_tag_opener_pos +
            string_find_first(string_init_substring(data, closing_tag_opener_pos, data.len), parse_tag_closer);
        String data_after_each =
            string_init_substring(data, closing_tag_closer_pos + parse_tag_closer.len, data.len);
        string_builder_add_string(a, &sb, _parse_string(a, data_after_each, mapping_lists, this_mapping));

        return string_builder_get_string(a, &sb);  // We are now done, so return early
        /*-------------------------------------------------------------------------------------------------------*/
      } else if (string_equals(command, string_literal("#sum"))) {
        /*------*/
        /* #sum */
        /*-------------------------------------------------------------------------------------------------------*/
        LinkNode *members = string_split(a, argument, string_literal("."));
        String list_name = link_node_get_container_node(members->next, StringNode, node)->data;

        for (LinkNode *p = mapping_lists.list_mappings->next; p != mapping_lists.list_mappings; p = p->next) {
          ListMapping this_list_mapping = link_node_get_container_node(p, ListMappingNode, node)->data;

          if (!string_equals(this_list_mapping.name, list_name)) {
            if (p->next == mapping_lists.list_mappings) {
              abort("No mapping found for #sum argument '%" Stringf "'", stringf_args(list_name));
            }
            continue;
          }

          U64 items_member_offset = 0;
          DisplayType items_display_type = this_list_mapping.item_display_type;
          String items_struct_name = this_list_mapping.item_struct_name;

          // Loop to get the location and display type of the desired member of the first item
          for (LinkNode *member_node = members->next->next; member_node != members;
               member_node = member_node->next) {
            String member_name = link_node_get_container_node(member_node, StringNode, node)->data;
            // Check through all the member mappings for one matching this name
            for (LinkNode *mapping_node = mapping_lists.member_mappings->next;
                 mapping_node != mapping_lists.member_mappings; mapping_node = mapping_node->next) {
              MemberMapping this_member_mapping =
                  link_node_get_container_node(mapping_node, MemberMappingNode, node)->data;

              if (string_equals(member_name, this_member_mapping.name) &&
                  string_equals(items_struct_name, this_member_mapping.struct_name)) {
                items_member_offset += this_member_mapping.offset;
                items_display_type = this_member_mapping.display_type;
                items_struct_name =
                    string_concat(a, 3, items_struct_name, string_literal("."), this_member_mapping.struct_name);
                break;
              }

              if (mapping_node->next == mapping_lists.member_mappings) {
                abort("No mapping found for member '%" Stringf "' of '%" Stringf "'", stringf_args(member_name),
                      stringf_args(items_struct_name));
              }
            }
          }

          union {
            F32 currency;
          } result = {0};

          // Now loop through each item and sum the result
          for (LinkNode *item_node = this_list_mapping.items->next; item_node != this_list_mapping.items;
               item_node = item_node->next) {
            // The list mapping contains a pointer to the linked list of the actual item values. Since we don't
            // know what type these are, we need to use the item node offset stored in the list mapping to get the
            // container node's data. We then add the member's offset to get a pointer to the member we want. If
            // I'm being completely honest, this code is rubbish
            void *item = (U8 *)item_node - this_list_mapping.item_node_offset + items_member_offset;

            switch (items_display_type) {
              case (DISPLAY_TYPE_CURRENCY): {
                result.currency += *((F32 *)item);
                break;
              }
              default: {
                abort("'%" Stringf "' has unsummable display type '%d'", stringf_args(items_struct_name),
                      (int)items_display_type);
              }
            }
          }

          String item_output;
          switch (items_display_type) {
            case (DISPLAY_TYPE_CURRENCY): {
              item_output = string_format(a, "%.02" F32f, result.currency);
              break;
            }
            default: {
              item_output = string_literal("");
            }
          }

          string_builder_add_string(a, &sb, item_output);
        }
        /*-------------------------------------------------------------------------------------------------------*/
      } else {
        abort("Unrecognised command '%" Stringf "'", stringf_args(command));
      }
      /*---------------------------------------------------------------------------------------------------------*/
    } else if (tag_contents.str[0] == '/') {
      /*------------*/
      /* / commands */
      /*---------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 1) {
        abort("Got %" U64f " arguments in end command tag (expected 1)", linked_list_get_length(tag_words));
      }
      String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;

      if (string_equals(command, string_literal("/each"))) {
        /*-------*/
        /* /each */
        /*-------------------------------------------------------------------------------------------------------*/
        // Do nothing for now

        /*-------------------------------------------------------------------------------------------------------*/
      } else {
        abort("Unrecognised command termination '% " Stringf "'", stringf_args(command));
      }
      /*---------------------------------------------------------------------------------------------------------*/
    } else {
      /*-------*/
      /* items */
      /*---------------------------------------------------------------------------------------------------------*/
      if (linked_list_get_length(tag_words) != 1) {
        abort("Got %" U64f " arguments in item tag (expected 1)", linked_list_get_length(tag_words));
      }

      String item_name = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;
      LinkNode *members = string_split(a, item_name, string_literal("."));

      void *item;
      DisplayType item_display_type;
      String item_struct_name;

      // Loop to get the location of the item, and its display type
      for (LinkNode *member_node = members->next; member_node != members; member_node = member_node->next) {
        String member_name = link_node_get_container_node(member_node, StringNode, node)->data;

        // The first loop will be an item mapping
        if (member_node == members->next) {
          if (string_equals(member_name, string_literal("this"))) {
            if (this_mapping.item == NULL) {
              abort("Improper use of 'this'");
            }
            item = this_mapping.item;
            item_display_type = this_mapping.display_type;
            item_struct_name = this_mapping.struct_name;
          } else {
            // Check through all the item mappings for one matching this name
            for (LinkNode *mapping_node = mapping_lists.item_mappings->next;
                 mapping_node != mapping_lists.item_mappings; mapping_node = mapping_node->next) {
              ItemMapping this_item_mapping =
                  link_node_get_container_node(mapping_node, ItemMappingNode, node)->data;

              if (string_equals(member_name, this_item_mapping.name)) {
                item = this_item_mapping.item;
                item_display_type = this_item_mapping.display_type;
                item_struct_name = this_item_mapping.struct_name;
                break;
              }

              if (mapping_node->next == mapping_lists.item_mappings) {
                abort("No mapping found for item '% " Stringf "'", stringf_args(member_name));
              }
            }
          }
        } else {  // After the first, each loop will be a member mapping
          // Check through all the member mappings for one matching this name
          for (LinkNode *mapping_node = mapping_lists.member_mappings->next;
               mapping_node != mapping_lists.member_mappings; mapping_node = mapping_node->next) {
            MemberMapping this_member_mapping =
                link_node_get_container_node(mapping_node, MemberMappingNode, node)->data;

            if (string_equals(member_name, this_member_mapping.name) &&
                string_equals(item_struct_name, this_member_mapping.struct_name)) {
              item = (U8 *)item + this_member_mapping.offset;
              item_display_type = this_member_mapping.display_type;
              item_struct_name =
                  string_concat(a, 3, item_struct_name, string_literal("."), this_member_mapping.struct_name);
              break;
            }

            if (mapping_node->next == mapping_lists.member_mappings) {
              abort("No mapping found for member '%" Stringf "' of '%" Stringf "'", stringf_args(member_name),
                    stringf_args(item_struct_name));
            }
          }
        }
      }

      String item_output;

      switch (item_display_type) {
        case DISPLAY_TYPE_NONE: {
          abort("Got item '%s' with no display type", item_name);
        }
        case DISPLAY_TYPE_CURRENCY: {
          item_output = string_format(a, "%.02" F32f, *((F32 *)item));
          break;
        }
        case DISPLAY_TYPE_DATE: {
          item_output = date_get_string(a, *((Date *)item), date_format, day_of_week_format);
          break;
        }
        case DISPLAY_TYPE_TEXT: {
          item_output = *((String *)item);
          break;
        }
      }

      string_builder_add_string(a, &sb, item_output);
      /*---------------------------------------------------------------------------------------------------------*/
    }

    cursor_pos = close_pos + parse_tag_closer.len;
  }

  string_builder_add_string(a, &sb, string_init_substring(data, cursor_pos, data.len));

  return string_builder_get_string(a, &sb);
}

String parse_string(Arena *a, String data, const MappingLists mapping_lists) {
  return _parse_string(a, data, mapping_lists, (ItemMapping){0});
}
