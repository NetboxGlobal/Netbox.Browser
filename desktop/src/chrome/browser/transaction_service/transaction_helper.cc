#include "chrome/browser/transaction_service/transaction_helper.h"


#include <math.h>
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"

namespace Netboxglobal
{


void json_helper_str(const base::Value& value, const std::string& key, std::string& output)
{
    const std::string* s = value.FindStringKey(key);
    if (s)
    {
        output = std::string(*s);
    }
}

void json_helper_int(const base::Value& value, const std::string& key, int& output)
{
    absl::optional<int> s_int = value.FindIntKey(key);
    if (s_int != absl::nullopt)
    {
        output = *s_int;
    }
}

void json_helper_amount(const base::Value& value, const std::string& key, int64_t& output)
{

    absl::optional<double> s_double = value.FindDoubleKey(key);
    if (s_double != absl::nullopt)
    {
        output = rint(*s_double * 100000000);
    }
}

std::map<std::string, TransactionData> parse_transactions_json(base::Value& json)
{
    std::map<std::string, TransactionData> results;

    if (!json.is_dict())
    {
        return results;
    }

    base::ListValue* transactions = nullptr;
    base::Value* transactions_raw = json.FindPath("transactions");
    if (!transactions_raw || !transactions_raw->GetAsList(&transactions) || !transactions)
    {
        return results;
    }

    // root = {"transactions":[{address", from:}]}
    // iterate
    //      amount = (int) amount * 10^8
    //      fee    = (int) fee * 10^8
    // group by txid, address, category, from
    // take first: confirmation_count, at, conflicted, blockhash

    base::Value::ListView transactions_view = transactions->GetList();

    for (unsigned int i = 0; i < transactions_view.size(); i++) 
    {
        TransactionData transaction;
    
        json_helper_str(transactions_view[i], "txid",              transaction.txid);
        json_helper_str(transactions_view[i], "address",           transaction.address_to);
        json_helper_str(transactions_view[i], "category",          transaction.category);
        json_helper_str(transactions_view[i], "from",              transaction.address_from);
        json_helper_str(transactions_view[i], "blockhash",         transaction.blockhash);
        transaction.blocknumber                 = 0;
        json_helper_int(transactions_view[i], "confirmations",     transaction.confirmations);
        if (transaction.confirmations > ENOUGH_CONFIRMATION_COUNT)
        {
            transaction.confirmations = ENOUGH_CONFIRMATION_COUNT;
        }
        json_helper_int(transactions_view[i], "time",              transaction.at);
        json_helper_amount(transactions_view[i], "amount",         transaction.amount);
        json_helper_amount(transactions_view[i], "fee",            transaction.fee);
        transaction.fee = std::abs(transaction.fee);


        const base::Value* conflicts = transactions_view[i].FindListKey("walletconflicts");
        transaction.conflicted = false;
        if (conflicts && !conflicts->GetList().empty())
        {
            transaction.conflicted = true;
        }

        auto r = results.find(transaction.key());
        if (r != results.end())
        {
            r->second.append(std::move(transaction));
        }
        else
        {
            results.insert({transaction.key(), std::move(transaction)});
        }
    }

    return results;
}

}
