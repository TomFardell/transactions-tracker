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
typedef enum DisplayType {
  DISPLAY_TYPE_NONE,
  DISPLAY_TYPE_CURRENCY,
  DISPLAY_TYPE_DATE,
  DISPLAY_TYPE_TEXT,
} DisplayType;

// Mapping of the start of a linked list
typedef struct ListMapping {
  LinkNode *items;                // Pointer to the list's link node
  String name;                    // Name of the list
  String item_struct_name;        // Struct name of each item (if needed to find members)
  DisplayType item_display_type;  // Display type of each item
  U64 item_node_offset;           // The offset of the node within each item in the linked list
} ListMapping;

// Mapping of a single item
typedef struct ItemMapping {
  void *item;                // Pointer to the item
  String name;               // Name of the item
  String struct_name;        // Struct name of the item (if needed to find members)
  DisplayType display_type;  // Display type of the item
} ItemMapping;

// Mapping of a member of some struct
typedef struct MemberMapping {
  U64 offset;                // Offset of the member within the struct
  String name;               // Name of the member
  String struct_name;        // Name of the struct containing the member
  DisplayType display_type;  // Display type of the member
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
void parse_file_into(String in_path, String out_path, const MappingLists mapping_lists);
// Parse the given string, storing the result on the given arena
String parse_string(Arena *a, String data, const MappingLists mapping_lists);

#endif  // PARSER_H

// vim: filetype=c :
