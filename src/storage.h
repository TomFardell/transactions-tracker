#ifndef STORAGE_H
#define STORAGE_H
#include <stdio.h>

#include "base/data.h"
#include "base/string.h"
#include "transaction.h"

// Write a string to a given file
void store_string(FILE *file, String str);
// Write a transaction to a given file
void store_transaction(FILE *file, Transaction trans);
// Store a linked list of transactions in a specified file
void store_transactions(const String file_name, LinkNode *transactions);

// Read a string from a given file, storing the data on the given arena
String retrieve_string(Arena *a, FILE *file);
// Read a transaction from a given file, storing the description string data on the given arena
Transaction retrieve_transaction(Arena *a, FILE *file);
// Retrieve transactions from a specified file, storing them in a linked list on the given arena
LinkNode *retrieve_transactions(Arena *a, String file_name);

#endif  // STORAGE_H

// vim: filetype=c :
