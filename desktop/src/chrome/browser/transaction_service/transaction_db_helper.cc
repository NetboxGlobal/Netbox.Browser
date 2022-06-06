#include "chrome/browser/transaction_service/transaction_db_helper.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_util.h"
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
            VLOG(NETBOX_LOG_LEVEL) << "failed to create directory " << db_folder_path_.value();
        }
    }

    #if defined(OS_WIN)
        return db_folder_path_.Append(base::UTF8ToWide(wallet_first_address));
    #else
        return db_folder_path_.Append(wallet_first_address);
    #endif
}

bool TransactionDBHelper::check_database(const std::string& wallet_first_address, bool recreate)
{
    if (db_.is_open())
    {
        db_.Close();
    }

    base::FilePath db_path = check_and_get_db_path(wallet_first_address);

    if (recreate)
    {
        if (base::PathExists(db_path))
        {
            if (!base::DeleteFile(db_path))
            {
                VLOG(NETBOX_LOG_LEVEL) << "failed to delete database " << db_path.value();
            }
        }
    }

    if (!db_.Open(db_path))
    {
        VLOG(1) << "failed to open db, " << db_path.value();
        return false;
    }

    if (wallet_first_address.empty())
    {
        VLOG(1) << "table name is empty";
        return false;
    }

    sql::Transaction committer(&db_);
    committer.Begin();

	std::string sql_create = "CREATE TABLE IF NOT EXISTS transactions"
						"("
							"txid TEXT NOT NULL,"
							"category TEXT NOT NULL,"
							"address_to TEXT NOT NULL,"
							"address_from TEXT NOT NULL,"
							"at INTEGER NOT NULL,"
							"amount INTEGER NOT NULL,"
							"fee INTEGER NOT NULL,"
							"confirmations INTEGER NOT NULL,"
							"conflicted INTEGER NOT NULL,"
							"blockhash TEXT NOT NULL,"
							"blocknumber INTEGER NOT NULL,"
							"PRIMARY KEY(txid, category, address_to, address_from)"
						")";
	// CREATE TABLE
	if (!db_.Execute(sql_create.c_str()))
	{
		VLOG(1) << "failed to create transactions table";
		return false;
	}

	std::vector<std::string> indexes_on_fields = {"at", "confirmations", "category,at", "address_to,at", "address_from,at"};
	for(auto field_name : indexes_on_fields)
	{
		std::string index_name = "transactions_" + field_name;
        base::ReplaceChars(index_name, ",", "_", &index_name);

		std::string index_sql = base::StringPrintf("CREATE INDEX IF NOT EXISTS %s ON transactions (%s)", index_name.c_str(), field_name.c_str());

		if (!db_.Execute(index_sql.c_str()))
		{
			VLOG(1) << "failed to create index, " << index_name;
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
		VLOG(1) << "failed to create settings";
		return false;
	}

    // INSERT VERSION
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)"));

    statement.BindString(0, "version");
    statement.BindString(1, "1");

    if (!statement.Run())
    {
		VLOG(1) << "failed to insert db version";
		return false;
    }

    committer.Commit();


    return true;
}

std::map<std::string, TransactionData> TransactionDBHelper::get_unconfirmed(const std::string wallet_first_address)
{
    std::map<std::string, TransactionData> results;

    sql::Statement query(db_.GetCachedStatement(SQL_FROM_HERE,
        "SELECT txid, category, address_to, address_from, confirmations, conflicted"
        " FROM transactions"
        " WHERE confirmations < " ENOUGH_CONFIRMATION_COUNT_STR " AND conflicted=0"
    ));

    // Execute the query and get results.
    while (query.is_valid() && query.Step())
    {
        TransactionData transaction;
        transaction.txid          = query.ColumnString(0);
        transaction.category      = query.ColumnString(1);
        transaction.address_to    = query.ColumnString(2);
        transaction.address_from  = query.ColumnString(3);
        transaction.confirmations = query.ColumnInt(4);
        transaction.conflicted    = query.ColumnInt(5);

        results.insert({transaction.key(), std::move(transaction)});
    }

    return results;
}

bool TransactionDBHelper::insert_or_update(const TransactionData& transaction)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "SELECT 1 FROM transactions "
        "WHERE txid=? AND category=? AND address_to=? AND address_from=?"));

    statement.BindString(0, transaction.txid);
    statement.BindString(1, transaction.category);
    statement.BindString(2, transaction.address_to);
    statement.BindString(3, transaction.address_from);

    if (statement.Step())
    {
        return update(transaction);
    }
    else
    {
        sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
            "INSERT INTO transactions"
            "(txid, category, address_to, address_from, at, amount, fee, confirmations, conflicted, blockhash, blocknumber) "
            "VALUES (?,?,?,?,?,?,?,?,?,?,?)"));

        statement.BindString(0,  transaction.txid);
        statement.BindString(1,  transaction.category);
        statement.BindString(2,  transaction.address_to);
        statement.BindString(3,  transaction.address_from);
        statement.BindInt(4,     transaction.at);
        statement.BindInt64(5,   transaction.amount);
        statement.BindInt64(6,   transaction.fee);
        statement.BindInt(7,     transaction.confirmations);
        statement.BindInt(8,     transaction.conflicted);
        statement.BindString(9,  transaction.blockhash);
        statement.BindInt(10,    transaction.blocknumber);

        return statement.Run();
    }
}

bool TransactionDBHelper::update(const TransactionData& transaction)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
                "UPDATE transactions "
                "SET confirmations=?, conflicted=?, blocknumber=?, blockhash=?, at=? "
                "WHERE txid=? AND category=? AND address_to=? AND address_from=?"));

    statement.BindInt(0,    transaction.confirmations);
    statement.BindInt(1,    transaction.conflicted);
    statement.BindInt(2,    transaction.blocknumber);
    statement.BindString(3, transaction.blockhash);
    statement.BindInt(4,    transaction.at);
    statement.BindString(5, transaction.txid);
    statement.BindString(6, transaction.category);
    statement.BindString(7, transaction.address_to);
    statement.BindString(8, transaction.address_from);

    return statement.Run();
}

bool TransactionDBHelper::set_conflicted(const TransactionData& transaction)
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "UPDATE transactions SET conflicted = 1 "
                                    "WHERE txid=? AND category=? AND address_to=? AND address_from=?"));

    statement.BindString(0, transaction.txid);
    statement.BindString(1, transaction.category);
    statement.BindString(2, transaction.address_to);
    statement.BindString(3, transaction.address_from);

    return statement.Run();
}

std::string TransactionDBHelper::get_latest_block()
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT value FROM settings WHERE key=?"));
    statement.BindString(0, "latest_block");

    if (!statement.Step())
    {
        return "";
    }

    return statement.ColumnString(0);
}

bool TransactionDBHelper::set_latest_block(const std::string& latest_block)
{
    sql::Transaction committer(&db_);
    committer.Begin();

    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)"));

    statement.BindString(0, "latest_block");
    statement.BindString(1, latest_block);

    if (!statement.Run())
    {
        return false;
    }

    return committer.Commit();
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
    std::string sql = "SELECT txid, category, address_to, address_from, at, amount, fee, confirmations, conflicted, blocknumber FROM transactions WHERE 1=1 ";

    // period_begin
    absl::optional<int> period_begin = params.FindIntKey("period_begin");
    if (period_begin != absl::nullopt && *period_begin > 0)
    {
        sql = sql + " AND at >= ?";
    }

    // period_finish
    absl::optional<int> period_finish = params.FindIntKey("period_finish");
    if (period_finish != absl::nullopt && *period_finish > 0)
    {
        sql = sql + " AND at <= ?";
    }

    // category
    const std::string* category = params.FindStringKey("category");
    if (category && category->size() > 0)
    {
        sql = sql + " AND category = ?";
    }

    // unconfirmed
    if (params.FindKey("unconfirmed"))
    {
        sql = sql + " AND conflicted = 0 AND confirmations < " ENOUGH_CONFIRMATION_COUNT_STR;
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
    const base::Value* addresses_to = params.FindListPath("addresses.address_to");
    const base::Value* addresses_from = params.FindListPath("addresses.address_from");
    if (addresses_to || addresses_from)
    {
        std::string address_sql = "";

        if (addresses_to && addresses_to->GetList().size() > 0)
        {
            address_sql = address_sql + " address_to IN (";
			for(unsigned int i = 0; i < addresses_to->GetList().size(); ++i)
			{
				if (0 == i)
				{
					address_sql = address_sql + "?";
				}
				else
				{
					address_sql = address_sql + ",?";
				}
			}

			 address_sql = address_sql + ")";
        }

		if (addresses_to
            && addresses_to->GetList().size() > 0
            && addresses_from
            && addresses_from->GetList().size() > 0)
		{
			address_sql = address_sql + " OR ";
		}

        if (addresses_from && addresses_from->GetList().size() > 0)
        {
            address_sql = address_sql + " address_from IN (";
			for(unsigned int i = 0; i < addresses_from->GetList().size(); ++i)
			{
				if (0 == i)
				{
					address_sql = address_sql + "?";
				}
				else
				{
					address_sql = address_sql + ",?";
				}
			}
			 address_sql = address_sql + ")";
        }

        if (address_sql.size() > 0)
        {
            sql = sql + " AND ( " + address_sql + ")";
        }
    }


    // text
    const std::string* text_raw = params.FindStringKey("text");
    if (text_raw && text_raw->size() > 0)
    {
        sql = sql + " AND (txid LIKE ? OR address_to LIKE ?)";
    }

    // order by
    sql = sql + " ORDER BY at DESC, txid";

    // limit
    absl::optional<int> limit = params.FindIntKey("limit");
    if (limit != absl::nullopt)
    {
        sql = sql + " LIMIT ?";
    }

    int bind_index = 0;
    sql::Statement statement(db_.GetUniqueStatement(sql.c_str()));

    if (period_begin != absl::nullopt && *period_begin > 0)
    {
        statement.BindInt(bind_index, *period_begin);
        bind_index++;
    }
    if (period_finish != absl::nullopt && *period_finish > 0)
    {
        statement.BindInt(bind_index, *period_finish);
        bind_index++;
    }
    if (category && category->size() > 0)
    {
        statement.BindString(bind_index, *category);
        bind_index++;
    }
    if (address_from && address_from->GetList().size() > 0)
    {
        for (const auto& address : address_from->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (address_to && address_to->GetList().size() > 0)
    {
        for (const auto& address : address_to->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (addresses_to && addresses_to->GetList().size() > 0)
    {
        for (const auto& address : addresses_to->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (addresses_from && addresses_from->GetList().size() > 0)
    {
        for (const auto& address : addresses_from->GetList())
        {
            statement.BindString(bind_index, address.GetString());
            bind_index++;
        }
    }

    if (text_raw && text_raw->size() > 0)
    {
        std::string text = *text_raw + "%";
        statement.BindString(bind_index, text);
        bind_index++;
        statement.BindString(bind_index, text);
        bind_index++;
    }

    if (limit != absl::nullopt)
    {
        statement.BindInt(bind_index, *limit);
        bind_index++;
    }

    base::Value transactions(base::Value::Type::LIST);
    while(statement.Step())
    {
        // txid, category, address_to,  address_from, at, amount, fee, confirmations, conflicted, blocknumber
        base::Value t(base::Value::Type::DICTIONARY);
        t.SetStringKey("txid",          statement.ColumnString(0));
        t.SetStringKey("category",      statement.ColumnString(1));
        t.SetStringKey("address_to",    statement.ColumnString(2));
        t.SetStringKey("address_from",  statement.ColumnString(3));
        t.SetIntKey("at",               statement.ColumnInt(4));
        t.SetStringKey("amount",        std::to_string(statement.ColumnInt64(5)));
        t.SetStringKey("fee",           std::to_string(statement.ColumnInt64(6)));
        t.SetIntKey("confirmations",    statement.ColumnInt(7));
        t.SetIntKey("conflicted",       statement.ColumnInt(8));
        t.SetIntKey("blocknumber",      statement.ColumnInt(9));

        transactions.Append(std::move(t));
    }

    return transactions;
}

int64_t TransactionDBHelper::get_balance()
{
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, "SELECT sum(amount) - sum(fee) FROM transactions WHERE conflicted=0"));

    if (!statement.Step())
    {
        return 0;
    }

    return statement.ColumnInt64(0);
}

}