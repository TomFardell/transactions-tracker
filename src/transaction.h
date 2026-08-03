#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "base/date.h"

typedef struct Transaction {
  String desc;
  Date date;
  F32 amount;
} Transaction;

define_node(Transaction);

// Initialise a transaction
Transaction transaction_init(String desc, Date date, F32 amount);

#endif  // TRANSACTION_H

// vim: filetype=c :
