#include "storage.h"

#include <stdio.h>

#include "base/memory.h"
#include "base/string.h"
#include "constants.h"
#include "helpers.h"
#include "transaction.h"

void store_string(FILE *file, String str) {
  error_check_fread_fwrite(fwrite(&str.len, sizeof(str.len), 1, file), file);
  error_check_fread_fwrite(fwrite(str.str, sizeof(*str.str), str.len, file), file);
}

void store_transaction(FILE *file, Transaction trans) {
  store_string(file, trans.desc);
  error_check_fread_fwrite(fwrite(&trans.date, sizeof(trans.date), 1, file), file);
  error_check_fread_fwrite(fwrite(&trans.amount, sizeof(trans.amount), 1, file), file);
}

void store_transactions(String file_name, const LinkNode *transactions) {
  Arena file_path_arena = arena_init(64);
  String file_path = string_append(&file_path_arena, data_dir, file_name);
  FILE *file = error_check_ptr(fopen(string_get_cstring(&file_path_arena, file_path), "w"));
  arena_free(&file_path_arena);

  foreach (transaction_node, transactions) {
    store_transaction(file, link_node_get_data(transaction_node, Transaction));
  }

  fclose(file);
}

String retrieve_string(Arena *a, FILE *file) {
  U64 result_len;
  if (!error_check_fread_fwrite(fread(&result_len, sizeof(result_len), 1, file), file)) {
    return (String){0};
  }

  char *result_str = arena_alloc_array(a, char, result_len);
  error_check_fread_fwrite(fread(result_str, sizeof(*result_str), result_len, file), file);

  return (String){result_str, result_len};
}

Transaction retrieve_transaction(Arena *a, FILE *file) {
  Transaction result;

  result.desc = retrieve_string(a, file);
  if (result.desc.str == 0) {
    return (Transaction){0};
  }
  error_check_fread_fwrite(fread(&result.date, sizeof(result.date), 1, file), file);
  error_check_fread_fwrite(fread(&result.amount, sizeof(result.amount), 1, file), file);

  return result;
}

LinkNode *retrieve_transactions(Arena *a, String file_name) {
  Arena file_path_arena = arena_init(64);
  String file_path = string_append(&file_path_arena, data_dir, file_name);
  FILE *file = error_check_ptr(fopen(string_get_cstring(&file_path_arena, file_path), "r"));
  arena_free(&file_path_arena);

  LinkNode *result = arena_alloc_single(a, LinkNode);
  linked_list_init(result);

  while (true) {
    Transaction transaction = retrieve_transaction(a, file);
    if (transaction.desc.str == 0) {
      return result;
    }

    TransactionNode *transaction_node = arena_alloc_single(a, TransactionNode);
    transaction_node->data = transaction;
    linked_list_push_back(result, &transaction_node->node);
  }
}
