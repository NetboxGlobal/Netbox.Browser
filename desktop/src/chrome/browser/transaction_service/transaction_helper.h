#ifndef CHROME_BROWSER_TRANSACTION_HELPER_H_
#define CHROME_BROWSER_TRANSACTION_HELPER_H_

#include <map>

#include "base/values.h"
#include "chrome/browser/transaction_service/transaction_model.h"

namespace Netboxglobal
{

std::map<std::string, TransactionData> parse_transactions_json(base::Value&);

}
#endif
