#include "chrome/browser/transaction_service/transaction_model.h"

namespace Netboxglobal
{


TransactionData::TransactionData()
{
}

TransactionData::~TransactionData()
{
}

TransactionData::TransactionData(TransactionData&& that)
{
    InternalMove(std::move(that));
}

TransactionData& TransactionData::operator=(TransactionData&& that)
{
    InternalMove(std::move(that));

    return *this;
}

void TransactionData::InternalMove(TransactionData&& data)
{
    txid            = data.txid;
    category        = data.category;
    txtype          = data.txtype;
    address_to      = data.address_to;
    address_from    = data.address_from;
    amount          = data.amount;
    fee             = data.fee;
    at              = data.at;
    height          = data.height;
}


}