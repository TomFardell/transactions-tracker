#ifndef PARSER_H
#define PARSER_H

#include "base/definitions.h"
#include "base/memory.h"
#include "base/string.h"

// Struct to be filled with pointers for the actual values that are being mapped in
typedef struct MappingInput {
  LinkNode *transactions;
} MappingInput;

// How to display the mapping
typedef enum DataType {
  DATA_TYPE_CURRENCY,
  DATA_TYPE_DATE,
  DATA_TYPE_TEXT,
} DataType;

// Mapping of the start of a linked list
typedef struct ListMapping {
  String name;
  LinkNode *items;
} ListMapping;

// Mapping of a single item
typedef struct ItemMapping {
  String name;
  void *item;
  DataType type;
} ItemMapping;

// Mapping of a displayable member of some struct
typedef struct MemberMapping {
  String name;
  U64 offset;
  DataType type;
} MemberMapping;

// Node containing a list mapping
typedef struct ListMappingNode {
  ListMapping data;
  LinkNode node;
} ListMappingNode;

// Node containing an item mapping
typedef struct ItemMappingNode {
  ItemMapping data;
  LinkNode node;
} ItemMappingNode;

// Node containing a member mapping
typedef struct MemberMappingNode {
  MemberMapping data;
  LinkNode node;
} MemberMappingNode;

// Storage of lists of each mapping type
typedef struct MappingLists {
  LinkNode *list_mappings;
  LinkNode *item_mappings;
  LinkNode *member_mappings;
} MappingLists;

// Initialise the mapping lists required by the parser, allocating them on the passed arena
MappingLists mapping_lists_init(Arena *a, MappingInput mapping_input);

// Given a file to parse, parse it and store the result in the out file
void parse_file_into(String in_path, String out_path, MappingLists mapping_lists);
// Parse the given string, storing the result on the given arena
String parse_string(Arena *a, String data, MappingLists mapping_lists);

#endif  // PARSER_H

// vim: filetype=c :
