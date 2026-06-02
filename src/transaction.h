#ifndef TRANSACTION_H
#define TRANSACTION_H
#include "base/date.h"

typedef struct Transaction {
  String desc;
  Date date;
  F32 amount;
} Transaction;

typedef struct TransactionNode {
  Transaction data;
  LinkNode node;
} TransactionNode;

// Initialise a transaction
Transaction transaction_init(String desc, Date date, F32 amount);

#endif  // TRANSACTION_H

// vim: filetype=c :
