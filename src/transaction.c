#include "transaction.h"

#include "base/date.h"

Transaction transaction_init(String desc, Date date, F32 amount) {
  return (Transaction){desc, date, amount};
}
