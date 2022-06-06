#ifndef CHROME_BROWSER_TRANSACTION_DB_HELPER_H_
#define CHROME_BROWSER_TRANSACTION_DB_HELPER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/transaction_service/transaction_model.h"
#include "sql/database.h"

namespace Netboxglobal
{


class TransactionDBHelper
{
public:
    TransactionDBHelper();
    ~TransactionDBHelper();

    void set_db_path(base::FilePath path);

    bool check_database(const std::string& wallet_first_address, bool recreate);
    std::map<std::string, TransactionData> get_unconfirmed(const std::string wallet_first_address);
    bool insert_or_update(const TransactionData&);
    bool update(const TransactionData&);
    bool set_conflicted(const TransactionData&);
    std::string get_latest_block();
    bool set_latest_block(const std::string& latest_block);
    base::Value get_transactions(const base::Value& params);
    int64_t get_balance();

    sql::Database* get_db();
    bool is_open();
private:
    base::Value get_transactions_internal(const base::Value& params);
    base::FilePath check_and_get_db_path(const std::string& wallet_first_address);

    base::FilePath db_folder_path_;
    sql::Database db_;
};

}

#endif