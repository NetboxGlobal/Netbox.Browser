#include "chrome/browser/transaction_service/transaction_db_helper.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace Netboxglobal
{


TransactionDBHelper::TransactionDBHelper()
{
}

TransactionDBHelper::~TransactionDBHelper()
{
    if (db_.is_open())
    {
        db_.Close();
    }
}

bool TransactionDBHelper::is_open()
{
    return db_.is_open();
}

sql::Database* TransactionDBHelper::get_db()
{
    return &db_;
}

void TransactionDBHelper::set_db_path(base::FilePath path)
{
    db_folder_path_ = path.Append(FILE_PATH_LITERAL("Wallet Data"));
}

base::FilePath TransactionDBHelper::check_and_get_db_path(const std::string& wallet_first_address)
{
    if (!base::DirectoryExists(db_folder_path_))
    {
        if (!base::CreateDirectory(db_folder_path_))
        {
            LOG(WARNING) << "failed to create directory " << db_folder_path_.value();
        }
    }

    #if defined(OS_WIN)
        return db_folder_path_.Append(base::UTF8ToUTF16(wallet_first_address));
    #else
        return db_folder_path_.Append(wallet_first_address);
    #endif
}

void TransactionDBHelper::remove_database(const std::string& wallet_first_address)
{
    if (db_.is_open())
    {
        db_.Close();
    }

    if (wallet_first_address.empty())
    {
        return;
    }

    base::FilePath db_path = check_and_get_db_path(wallet_first_address);

    if (base::PathExists(db_path))
    {
        if (!base::DeleteFile(db_path))
        {
            LOG(WARNING) << "failed to delete database " << db_path.value();
        }
    }
}

bool TransactionDBHelper::create_database_if_not_exists(const std::string& wallet_first_address)
{
    if (db_.is_open())
    {
        db_.Close();
    }

    if (wallet_first_address.empty())
    {
        return false;
    }

    base::FilePath db_path = check_and_get_db_path(wallet_first_address);

    if (!db_.Open(db_path))
    {
        LOG(WARNING) << "failed to open db, " << db_path.value();
        return false;
    }

    sql::Transaction committer(&db_);
    committer.Begin();

	std::string sql_create = "CREATE TABLE IF NOT EXISTS transactions"
						"("
							"txid TEXT NOT NULL,"
                            "txtype INTEGER NOT NULL,"
                            "category TEXT NOT NULL,"
							"address_to TEXT NOT NULL,"
							"address_from TEXT NOT NULL,"
                            "at INTEGER NOT NULL,"
                            "height INTEGER NOT NULL,"
							"amount INTEGER NOT NULL,"
							"fee INTEGER NOT NULL,"
							"PRIMARY KEY(txid)"
						")";
	// CREATE TABLE
	if (!db_.Execute(sql_create.c_str()))
	{
		LOG(WARNING) << "failed to create transactions table";
		return false;
	}

	std::vector<std::string> indexes_on_fields = {"at", "category,at", "address_to,at", "address_from,at"};
	for(auto field_name : indexes_on_fields)
	{
		std::string index_name = "transactions_" + field_name;
        base::ReplaceChars(index_name, ",", "_", &index_name);

		std::string index_sql = base::StringPrintf("CREATE INDEX IF NOT EXISTS %s ON transactions (%s)", index_name.c_str(), field_name.c_str());

		if (!db_.Execute(index_sql.c_str()))
		{
			LOG(WARNING) << "failed to create index, " << index_name;
			return false;
		}
	}

    sql_create = "CREATE TABLE IF NOT EXISTS settings"
						"("
							"key TEXT NOT NULL,"
							"value TEXT NOT NULL,"
							"PRIMARY KEY(key)"
						")";
	// CREATE TABLE
	if (!db_.Execute(sql_create.c_str()))
	{
		LOG(WARNING) << "failed to create settings";
		return false;
	}

    // INSERT VERSION
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)"));

    statement.BindString(0, "version");
    statement.BindString(1, "1");

    if (!statement.Run())
    {
		LOG(WARNING) << "failed to insert db version";
		return false;
    }

    committer.Commit();


    return true;
}

std::string TransactionDBHelper::get_last_confirmed_txid(int current_height)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "SELECT txid FROM transactions"
        " WHERE ? - height >= 100"
        " ORDER BY height DESC, at LIMIT 1"
    ));

    statement.BindInt(0, current_height);

    if (statement.Step())
    {
        return statement.ColumnString(0);
    }

    return "";
}

bool TransactionDBHelper::insert_or_update(const TransactionData& transaction)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "SELECT 1 FROM transactions "
        "WHERE txid=?"));

    statement.BindString(0, transaction.txid);

    if (statement.Step())
    {
        return update(transaction);
    }
    else
    {
        sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
            "INSERT INTO transactions"
            "(txid, category, txtype, address_to, address_from, amount, fee, at, height) "
            "VALUES (?,?,?,?,?,?,?,?,?)"));

        statement.BindString(0, transaction.txid);
        statement.BindString(1, transaction.category);
        statement.BindInt(2,    transaction.txtype);
        statement.BindString(3, transaction.address_to);
        statement.BindString(4, transaction.address_from);
        statement.BindInt64(5,  transaction.amount);
        statement.BindInt64(6,  transaction.fee);
        statement.BindInt(7,    transaction.at);
        statement.BindInt(8,    transaction.height);

        if (statement.Run())
        {
            return true;
        }

        LOG(WARNING) << "insert failed ";
        return false;
    }
}

bool TransactionDBHelper::update(const TransactionData& transaction)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
                "UPDATE transactions "
                "SET category=?, txtype=?, address_to=?, address_from=?, amount=?, fee=?, at=?, height=? "
                "WHERE txid=?"));


    statement.BindString(0,  transaction.category);
    statement.BindInt(1,     transaction.txtype);
    statement.BindString(2,  transaction.address_to);
    statement.BindString(3,  transaction.address_from);
    statement.BindInt64(4,   transaction.amount);
    statement.BindInt64(5,   transaction.fee);
    statement.BindInt(6,     transaction.at);
    statement.BindInt(7,    transaction.height);
    statement.BindString(8,  transaction.txid);

    if (statement.Run())
    {
        return true;
    }

    LOG(WARNING) << "update failed";
    return false;
}

base::Value TransactionDBHelper::get_transactions(const base::Value& params)
{
    if (!db_.is_open())
    {
        base::Value error_result(base::Value::Type::DICTIONARY);
        error_result.SetStringKey("error", "Database is not open for transactions request");

        return error_result;
    }

    base::Value transactions = get_transactions_internal(params);

    // get transactions
    base::Value result(base::Value::Type::DICTIONARY);
    result.SetKey("transactions", std::move(transactions));

    // get max/min dates
    sql::Statement max_min_statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT min(at), max(at) FROM transactions"));
    if (max_min_statement.Step())
    {
        if (max_min_statement.ColumnInt(0) > 0)
        {
            result.SetIntKey("first_transaction_date", max_min_statement.ColumnInt(0));
        }
        if (max_min_statement.ColumnInt(1) > 0)
        {
            result.SetIntKey("last_transaction_date", max_min_statement.ColumnInt(1));
        }
    }

    // get pending
    const base::Value* staking_pending_to = params.FindKey("staking_pending_to");
    if (staking_pending_to && staking_pending_to->is_dict())
    {
        base::Value staking_pending = get_transactions_internal(*staking_pending_to);
        result.SetKey("staking_pending", std::move(staking_pending));
    }

    const base::Value* lottery_pending_to = params.FindKey("lottery_pending_to");
    if (lottery_pending_to && lottery_pending_to->is_dict())
    {
        base::Value lottery_pending = get_transactions_internal(*lottery_pending_to);
        result.SetKey("lottery_pending", std::move(lottery_pending));
    }

    return result;
}

base::Value TransactionDBHelper::get_transactions_internal(const base::Value& params)
{
    std::string sql = "SELECT txid, category, txtype, address_to, address_from, at, amount, fee, height FROM transactions WHERE 1=1 ";

    int height = 0;
    base::Optional<int> height_raw = params.FindIntKey("height");
    if (base::nullopt != height_raw)
    {
        height = *height_raw;
    }

    // period_begin
    base::Optional<int> period_begin = params.FindIntKey("period_begin");
    if (period_begin != base::nullopt)
    {
        sql = sql + " AND at >= ?";
    }

    // period_finish
    base::Optional<int> period_finish = params.FindIntKey("period_finish");
    if (period_finish != base::nullopt)
    {
        sql = sql + " AND at <= ?";
    }

    // category
    const std::string* category = params.FindStringKey("category");
    if (category)
    {
        sql = sql + " AND category = ?";
    }

    // unconfirmed
    if (params.FindKey("unconfirmed"))
    {
        //sql = sql + " AND confirmations < " ENOUGH_CONFIRMATION_COUNT_STR;
    }

    // address_from in [...]
    const base::Value* address_from = params.FindListKey("address_from");
    if (address_from)
    {
        sql = sql + " AND address_from IN (";
        for(unsigned int i = 0; i < address_from->GetList().size(); ++i)
        {
            if (0 == i)
            {
                sql = sql + "?";
            }
            else
            {
                sql = sql + ",?";
            }
        }
        sql = sql + ")";
    }

    // address_to in [...]
    const base::Value* address_to = params.FindListKey("address_to");
    if (address_to)
    {
        sql = sql + " AND address_to IN (";
        for(unsigned int i = 0; i < address_to->GetList().size(); ++i)
        {
            if (0 == i)
            {
                sql = sql + "?";
            }
            else
            {
                sql = sql + ",?";
            }
        }
        sql = sql + ")";
    }

    // addresses
    const base::Value* addresses_to = params.FindPath("addresses.address_to");
    const base::Value* addresses_from = params.FindPath("addresses.address_from");
    if (addresses_to
     && addresses_from)
    {
        if (0 == addresses_to->GetList().size()
         && 0 == addresses_from->GetList().size())
        {
            return base::Value(base::Value::Type::LIST);
        }

        if (addresses_to->GetList().size() > 0 || addresses_from->GetList().size() > 0)
        {
            sql = sql + " AND ( ";

            if (addresses_to->GetList().size() > 0)
            {
                sql = sql + " address_to IN (";
                for(unsigned int i = 0; i < addresses_to->GetList().size(); ++i)
                {
                    if (0 == i)
                    {
                        sql = sql + "?";
                    }
                    else
                    {
                        sql = sql + ",?";
                    }
                }

                 sql = sql + ")";
            }

            if (addresses_to->GetList().size() > 0 && addresses_from->GetList().size() > 0)
            {
                sql = sql + " OR ";
            }

            if (addresses_from->GetList().size() > 0)
            {
                sql = sql + " address_from IN (";
                for(unsigned int i = 0; i < addresses_from->GetList().size(); ++i)
                {
                    if (0 == i)
                    {
                        sql = sql + "?";
                    }
                    else
                    {
                        sql = sql + ",?";
                    }
                }
                 sql = sql + ")";
            }

            sql = sql + ")";
        }
    }


    // order by
    sql = sql + " ORDER BY height DESC, at DESC, txid DESC";

    // limit
    base::Optional<int> limit = params.FindIntKey("limit");
    if (limit != base::nullopt)
    {
        sql = sql + " LIMIT ?";
    }

    int bind_index = 0;
    sql::Statement statement(db_.GetUniqueStatement(sql.c_str()));

    if (period_begin != base::nullopt)
    {
        statement.BindInt(bind_index, *period_begin);
        bind_index++;
    }
    if (period_finish != base::nullopt)
    {
        statement.BindInt(bind_index, *period_finish);
        bind_index++;
    }
    if (category)
    {
        statement.BindString(bind_index, *category);
        bind_index++;
    }
    if (address_from)
    {
        for (const auto& address : address_from->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (address_to)
    {
        for (const auto& address : address_to->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (addresses_to)
    {
        for (const auto& address : addresses_to->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (addresses_from)
    {
        for (const auto& address : addresses_from->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (limit != base::nullopt)
    {
        statement.BindInt(bind_index, *limit);
        bind_index++;
    }

    int i =0;

    base::Value transactions(base::Value::Type::LIST);
    while(statement.Step())
    {

        base::Value t(base::Value::Type::DICTIONARY);
        t.SetStringKey("txid",          statement.ColumnString(0));
        t.SetStringKey("category",      statement.ColumnString(1));
        t.SetIntKey("txtype",           statement.ColumnInt(2));
        t.SetStringKey("address_to",    statement.ColumnString(3));
        t.SetStringKey("address_from",  statement.ColumnString(4));
        t.SetIntKey("at",               statement.ColumnInt(5));
        t.SetStringKey("amount",        std::to_string(statement.ColumnInt64(6)));
        if ("send" == statement.ColumnString(1))
        {
            t.SetStringKey("fee",           std::to_string(statement.ColumnInt64(7)));
        }

        int transaction_height          = statement.ColumnInt(8);
        t.SetIntKey("height",           transaction_height);

        if (height >= transaction_height)
        {
            t.SetIntKey("confirmations",    height - transaction_height);
        }
        else
        {
            t.SetIntKey("confirmations", 0);
        }


        transactions.Append(std::move(t));

        i++;
    }

    return transactions;
}

int64_t TransactionDBHelper::get_balance()
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT sum(amount) - sum(fee) FROM transactions"));

    if (!statement.Step())
    {
        return 0;
    }

    return statement.ColumnInt64(0);
}

void TransactionDBHelper::delete_transactions()
{
    sql::Statement statement1(db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM transactions"));
    statement1.Run();

    sql::Statement statement2(db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM settings"));
    statement2.Run();
}

void TransactionDBHelper::desync()
{
    sql::Statement statement_1(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT count(*) FROM transactions"));

    if (statement_1.Step())
    {
        LOG(WARNING) << "desync 1/3:" << statement_1.ColumnInt64(0);
    }

    sql::Transaction committer(&db_);
    committer.Begin();

    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM transactions WHERE amount > 0"));

    if (statement.Run())
    {
        LOG(WARNING) << "desync 2/3:ok";
    }

    committer.Commit();

    sql::Statement statement_2(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT count(*) FROM transactions "));

    if (statement_2.Step())
    {
        LOG(WARNING) << "desync 3/3:" << statement_2.ColumnInt64(0);
    }
}

base::Value TransactionDBHelper::dump()
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT txid, category, txtype, address_to, address_from, at, amount, fee, height FROM transactions ORDER BY height, at DESC, txid"));

    base::Value transactions(base::Value::Type::LIST);
    while(statement.Step())
    {

        base::Value t(base::Value::Type::DICTIONARY);
        t.SetStringKey("txid",          statement.ColumnString(0));
        t.SetStringKey("category",      statement.ColumnString(1));
        t.SetIntKey("txtype",           statement.ColumnInt(2));
        t.SetStringKey("address_to",    statement.ColumnString(3));
        t.SetStringKey("address_from",  statement.ColumnString(4));
        t.SetIntKey("at",               statement.ColumnInt(5));
        t.SetStringKey("amount",        std::to_string(statement.ColumnInt64(6)));
        t.SetStringKey("fee",           std::to_string(statement.ColumnInt64(7)));
        t.SetIntKey("height",           statement.ColumnInt(8));

        transactions.Append(std::move(t));
    }

    return transactions;

}

bool TransactionDBHelper::is_resync_allowed()
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT count(*) FROM transactions WHERE at > ?"));

    statement.BindInt64(0, base::Time::Now().ToTimeT() - 30 * 60);

    if (statement.Step() && 0 == statement.ColumnInt(0))
    {
        return true;
    }

    return false;
}

#define BLOCK_HASH_FIELD_NAME "block_hash"

bool TransactionDBHelper::get_last_block_hash(std::string& block_hash)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT value FROM settings WHERE key=?"));
    statement.BindString(0, BLOCK_HASH_FIELD_NAME);

    if (!statement.Step())
    {
        LOG(WARNING) << "still empty block hash";
        return false;
    }

    block_hash = statement.ColumnString(0);
    return true;
}

bool TransactionDBHelper::is_block_hash_changed(std::string block_hash)
{
    std::string block_hash_local;
    if (!get_last_block_hash(block_hash_local))
    {
        return true;
    }

    if (block_hash == block_hash_local)
    {
        return false;
    }

    return true;
}

void  TransactionDBHelper::update_block_hash(std::string block_hash)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT value FROM settings WHERE key=?"));
    statement.BindString(0, BLOCK_HASH_FIELD_NAME);

    if (statement.Step())
    {
        sql::Statement update_statement(db_.GetCachedStatement(SQL_FROM_HERE, "UPDATE settings SET value = ? WHERE key = ? "));

        update_statement.BindString(0, block_hash);
        update_statement.BindString(1, BLOCK_HASH_FIELD_NAME);

        if (!update_statement.Run())
        {
            LOG(WARNING) << "Failed to update/update_block_hash";
        }
    }
    else
    {

        sql::Statement insert_statement(db_.GetCachedStatement(SQL_FROM_HERE, "INSERT INTO settings (key, value) VALUES(?, ?)"));

        insert_statement.BindString(0, BLOCK_HASH_FIELD_NAME);
        insert_statement.BindString(1, block_hash);

        if (!insert_statement.Run())
        {
            LOG(WARNING) << "Failed to insert/update_block_hash";
        }
    }
}


}