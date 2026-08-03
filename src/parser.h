#ifndef PARSER_H
#define PARSER_H

#include "base/definitions.h"
#include "base/memory.h"
#include "base/string.h"

// Struct to be filled with pointers for the underlying data that is being mapped into
typedef struct MappingInput {
  LinkNode *transactions;
  U32 *num_add_transaction_inputs;
} MappingInput;

// How to display the mapping
typedef enum DisplayType {
  DISPLAY_TYPE_NONE,
  DISPLAY_TYPE_CURRENCY,
  DISPLAY_TYPE_DATE,
  DISPLAY_TYPE_NUMBER,
  DISPLAY_TYPE_TEXT,
} DisplayType;

// How the data is stored in the program
typedef enum InternalType {
  INTERNAL_TYPE_NULL,
  INTERNAL_TYPE_DATE,
  INTERNAL_TYPE_F32,
  INTERNAL_TYPE_F64,
  INTERNAL_TYPE_STRING,
  INTERNAL_TYPE_TRANSACTION,
  INTERNAL_TYPE_U32,
} InternalType;

// Mapping of the start of a linked list
typedef struct ListMapping {
  LinkNode *items;                  // Pointer to the list's link node
  String name;                      // Name of the list
  String item_struct_name;          // Struct name of each item (if needed to find members)
  DisplayType item_display_type;    // Display type of each item
  InternalType item_internal_type;  // Internal type of each item
} ListMapping;

// Mapping of a single item
typedef struct ItemMapping {
  void *item;                  // Pointer to the item
  String name;                 // Name of the item
  String struct_name;          // Struct name of the item (if needed to find members)
  DisplayType display_type;    // Display type of the item
  InternalType internal_type;  // Internal type of the item
} ItemMapping;

// Mapping of a member of some struct
typedef struct MemberMapping {
  U64 offset;                  // Offset of the member within the struct
  String name;                 // Name of the member
  String struct_name;          // Name of the struct containing the member
  DisplayType display_type;    // Display type of the member
  InternalType internal_type;  // Internal type of the member
} MemberMapping;

define_node(ListMapping);
define_node(ItemMapping);
define_node(MemberMapping);

// Storage of lists of each mapping type
typedef struct MappingLists {
  LinkNode *list_mappings;
  LinkNode *item_mappings;
  LinkNode *member_mappings;
} MappingLists;

// Initialise the mapping lists required by the parser, allocating them on the passed arena
MappingLists mapping_lists_init(Arena *a, MappingInput mapping_input);

// Locate a list mapping of a given name in the passed mapping lists. Returns a list mapping with a null item
// internal type if not located
ListMapping mapping_lists_locate_list_mapping(MappingLists mapping_lists, String list_mapping_name);
// Locate an item mapping of a given name in the passed mapping lists. Returns an item mapping with a null internal
// type if not located
ItemMapping mapping_lists_locate_item_mapping(MappingLists mapping_lists, String item_mapping_name);
// Locate a matching struct mapping in the passed mapping lists. Returns a member mapping with a null internal type
// if not located
MemberMapping mapping_lists_locate_member_mapping(MappingLists mapping_lists, String member_mapping_struct_name,
                                                  String member_mapping_name);
// Given the name of a struct, return a list of all members of that struct that have a mapping set up
LinkNode *mapping_lists_get_member_mappings_for_struct(Arena *a, MappingLists mapping_lists, String struct_name);

// Parse the given string, storing the result on the given arena
String parse_string(Arena *a, String data, const MappingLists mapping_lists);
// Given a file to parse, parse it and store the result in the out file
void parse_file_into(String in_path, String out_path, const MappingLists mapping_lists);

#endif  // PARSER_H

// vim: filetype=c :
