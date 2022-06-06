#ifndef CHROME_BROWSER_TRANSACTION_MODEL_H_
#define CHROME_BROWSER_TRANSACTION_MODEL_H_

#include <string>

#include "base/macros.h"

namespace Netboxglobal
{


const int TXTYPE_SEND = 1;
const int TXTYPE_RECEIVE = 2;
const int TXTYPE_POW = 4;
const int TXTYPE_POS = 8;
const int TXTYPE_STAKE = 16;
const int TXTYPE_MASTERNODE = 32;

class TransactionData{
public:
    TransactionData();
    ~TransactionData();
    TransactionData(TransactionData&&);
    TransactionData& operator=(TransactionData&&);

    std::string txid = "";
    std::string category = "";
    int txtype = 0;
    std::string address_to = "";
    std::string address_from = "";
    int64_t amount = 0;
    int64_t fee = 0;
    int at = 0;
    int height = 0;


private:
    void InternalMove(TransactionData&& data);

    DISALLOW_COPY_AND_ASSIGN(TransactionData);
};

}
#endif
