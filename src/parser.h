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

// Type of mapping held in a mapping node
typedef enum MappingType {
  MAPPING_TYPE_LIST,
  MAPPING_TYPE_ITEM,
  MAPPING_TYPE_MEMBER,
} MappingType;

// Node used in a linked list of mappings
typedef struct MappingNode {
  MappingType type;
  union {
    ListMapping list_mapping;
    ItemMapping item_mapping;
    MemberMapping member_mapping;
  };
  LinkNode node;
} MappingNode;

// Initialise the mapping data required for the parser - a linked list of mappings allocated on the passed arena
LinkNode *mapping_init(Arena *a, MappingInput mapping_input);

// Given a file to parse, parse it and store the result in the out file
void parse_file_into(String file_path_in, String file_path_out, const LinkNode *mapping);
// Parse the given string, storing the result on the given arena
String parse_string(Arena *a, String data, const LinkNode *mapping);

#endif  // PARSER_H

// vim: filetype=c :
