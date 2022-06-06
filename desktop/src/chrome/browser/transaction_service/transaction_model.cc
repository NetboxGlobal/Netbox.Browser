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
    address_to      = data.address_to;
    category        = data.category;
    address_from    = data.address_from;
    blocknumber     = data.blocknumber;
    blockhash       = data.blockhash;
    amount          = data.amount;
    fee             = data.fee;
    at              = data.at;
    confirmations   = data.confirmations;
    conflicted      = data.conflicted;

}

void TransactionData::append(const TransactionData& data)
{
    amount  = amount + data.amount;
    fee     = fee + data.fee;
}

std::string TransactionData::key()
{
    return txid + "-" + address_to + "-" + category + address_from;
}

bool TransactionData::is_same_without_address_from(const TransactionData& data)
{
    if (txid == data.txid
     && address_to == data.address_to
     && category == data.category
     && amount == data.amount)
    {
        return true;
    }

    return false;

}

}