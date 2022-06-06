#ifndef CHROME_BROWSER_TRANSACTION_MODEL_H_
#define CHROME_BROWSER_TRANSACTION_MODEL_H_

#include <string>

#include "base/macros.h"

#define ENOUGH_CONFIRMATION_COUNT 101
#define ENOUGH_CONFIRMATION_COUNT_STR "101"

namespace Netboxglobal
{

class TransactionData{
public:
    TransactionData();
    ~TransactionData();
    TransactionData(TransactionData&&);
    TransactionData& operator=(TransactionData&&);

    std::string txid = "";
    std::string address_to = "";
    std::string category = "";
    std::string address_from = "";
    int blocknumber = 0;
    std::string blockhash = "";
    int64_t amount = 0;
    int64_t fee = 0;
    int at = 0;
    int confirmations = 0;
    bool conflicted = false;


    void append(const TransactionData& d);
    std::string key();
    bool is_same_without_address_from(const TransactionData& data);

private:
    void InternalMove(TransactionData&& data);

    DISALLOW_COPY_AND_ASSIGN(TransactionData);
};

}
#endif
