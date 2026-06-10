#include "parser.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/definitions.h"
#include "base/memory.h"
#include "base/string.h"
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
  list_mapping_node->data.name = string_literal("transactions");
  list_mapping_node->data.items = mapping_input.transactions;
  linked_list_push_back(result.list_mappings, &list_mapping_node->node);

  MemberMappingNode *member_mapping_node;

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.name = string_literal("desc");
  member_mapping_node->data.offset = offset_of(Transaction, desc);
  member_mapping_node->data.type = DATA_TYPE_TEXT;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.name = string_literal("date");
  member_mapping_node->data.offset = offset_of(Transaction, date);
  member_mapping_node->data.type = DATA_TYPE_DATE;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  member_mapping_node = arena_alloc_single(a, MemberMappingNode);
  member_mapping_node->data.name = string_literal("amount");
  member_mapping_node->data.offset = offset_of(Transaction, amount);
  member_mapping_node->data.type = DATA_TYPE_CURRENCY;
  linked_list_push_back(result.member_mappings, &member_mapping_node->node);

  return result;
}

void parse_file_into(String in_path, String out_path, MappingLists mapping_lists) {
  Arena parse_arena = arena_init(8192);

  I32 infd = open(string_get_cstring(&parse_arena, in_path), O_RDONLY);
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

String parse_string(Arena *a, String data, MappingLists mapping_lists) {
  LinkNode *opening_tags = string_find_all(a, data, string_literal("{{"));
  LinkNode *closing_tags = string_find_all(a, data, string_literal("}}"));

  if (linked_list_get_length(opening_tags) != linked_list_get_length(closing_tags)) {
    abort("Different number of '{{' (%" U64f ") and '}}' (% " U64f ")", linked_list_get_length(opening_tags),
          linked_list_get_length(closing_tags));
  }

  LinkNode sb;
  linked_list_init(&sb);
  U64 cursor_pos = 0;

  for (LinkNode *open_node = opening_tags->next, *close_node = closing_tags->next; open_node != opening_tags;
       open_node = open_node->next, close_node = close_node->next) {
    U64 open_pos = link_node_get_container_node(open_node, U64Node, node)->data;
    U64 close_pos = link_node_get_container_node(close_node, U64Node, node)->data;

    string_builder_add_string(a, &sb, string_init_substring(data, cursor_pos, open_pos));
    String tag_contents = string_init_substring(data, open_pos + 2, close_pos);

    LinkNode *tag_words = string_split(a, tag_contents, string_literal(" "));
    if (tag_contents.str[0] == '#') {
      if (linked_list_get_length(tag_words) != 2) {
        abort("Got %" U64f " arguments in command tag (expected 2)", linked_list_get_length(tag_words));
      }
      String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;
      String argument = linked_list_get_container_node_at_index(tag_words, 1, StringNode, node)->data;

      if (string_equals(command, string_literal("#each"))) {
        string_builder_add_string(a, &sb, string_literal("<each command over"));
        for (LinkNode *p = mapping_lists.list_mappings->next; p != mapping_lists.list_mappings; p = p->next) {
          ListMapping this_list_mapping = link_node_get_container_node(p, ListMappingNode, node)->data;
          if (!string_equals(this_list_mapping.name, argument)) {
            if (p->next == mapping_lists.list_mappings) {
              abort("No mapping found for #each argument %s", string_get_cstring(a, argument));
            }
            continue;
          }

          for (LinkNode *q = this_list_mapping.items->next; q != this_list_mapping.items; q = q->next) {
            string_builder_add_string(
                a, &sb, string_format(a, " %p", &link_node_get_container_node(q, TransactionNode, node)->data));
          }
        }

        string_builder_add_string(a, &sb, string_literal(">"));
      } else if (string_equals(command, string_literal("#sum"))) {
        string_builder_add_string(a, &sb, string_literal("<sum "));
        string_builder_add_string(a, &sb, argument);
        string_builder_add_string(a, &sb, string_literal(" command>"));
      }
    } else if (tag_contents.str[0] == '/') {
      if (linked_list_get_length(tag_words) != 1) {
        abort("Got %" U64f " arguments in end command tag (expected 1)", linked_list_get_length(tag_words));
      }
      String command = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;

      if (string_equals(command, string_literal("/each"))) {
        string_builder_add_string(a, &sb, string_literal("<end each command>"));
      }
    } else {
      if (linked_list_get_length(tag_words) != 1) {
        abort("Got %" U64f " arguments in item tag (expected 1)", linked_list_get_length(tag_words));
      }
      String item = linked_list_get_container_node_at_index(tag_words, 0, StringNode, node)->data;
      string_builder_add_string(a, &sb, string_literal("<item "));
      string_builder_add_string(a, &sb, item);
      string_builder_add_string(a, &sb, string_literal(">"));
    }

    cursor_pos = close_pos + 2;
  }

  string_builder_add_string(a, &sb, string_init_substring(data, cursor_pos, data.len));

  return string_builder_get_string(a, &sb);
}
