#include "chrome/browser/transaction_service/transaction_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/wallet_manager/wallet_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "sql/transaction.h"

namespace Netboxglobal
{

const int MAX_TRANSACTIONS = 100;

TransactionMobileService::TransactionMobileService()
{
    DETACH_FROM_SEQUENCE(sequence_checker_);

    db_helper_.reset(new TransactionDBHelper());
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

TransactionMobileService::~TransactionMobileService()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui_stop();
}

void TransactionMobileService::ui_stop()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui_requests_.clear();
    task_runner_.reset();
    db_helper_.reset();
}

void TransactionMobileService::ui_pre_start(base::FilePath profile_path)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionMobileService::db_set_path, base::Unretained(this),
                         profile_path));
}

void TransactionMobileService::db_set_path(base::FilePath db_path)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    db_helper_->set_db_path(db_path);
}

void TransactionMobileService::ui_set_first_address(std::string wallet_first_address, bool is_qa)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                        base::BindOnce(&TransactionMobileService::db_set_first_address,
                            base::Unretained(this),
                            std::move(wallet_first_address),
                            is_qa));

}

void TransactionMobileService::db_set_first_address(std::string wallet_first_address, bool is_qa)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    db_is_qa_ = is_qa;

    db_wallet_first_address_ = wallet_first_address;

    if (!db_helper_->create_database_if_not_exists(db_wallet_first_address_))
    {
        VLOG(1) << "Failed to open database for " << db_wallet_first_address_;
    }
}

void TransactionMobileService::ui_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> external_signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                        base::BindOnce(&TransactionMobileService::db_get_last_block_hash, base::Unretained(this), std::move(external_signature)));
}

void TransactionMobileService::db_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> external_signature)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::Value event_result(base::Value::Type::DICTIONARY);
    std::string block_hash;

    if (db_helper_->get_last_block_hash(block_hash))
    {
        event_result.SetStringKey("last_block_hash", block_hash);
    }
    else
    {
        event_result.SetBoolKey("last_block_hash", false);
    }

    IWalletTabHandler* handler  = external_signature->get_ui_handler();
    std::string event_name      = external_signature->get_event_name();

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
            handler,
            std::move(event_name),
            std::move(event_result)
        )
    );
}

void TransactionMobileService::ui_import_transactions(std::unique_ptr<WalletHttpCallSignature> external_signature, int current_height, std::string block_hash)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                        base::BindOnce(&TransactionMobileService::check_block_hash, base::Unretained(this), std::move(external_signature), current_height, block_hash));
}

void TransactionMobileService::check_block_hash(std::unique_ptr<WalletHttpCallSignature> external_signature, int current_height, std::string block_hash)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (db_helper_->is_block_hash_changed(block_hash))
    {
        external_signature->append_extra_data("block_hash", base::Value(block_hash));

        db_request(std::move(external_signature), current_height);
    }
    else
    {
        IWalletTabHandler* handler  = external_signature->get_ui_handler();
        std::string event_name      = external_signature->get_event_name();
        base::Value event_result(base::Value::Type::DICTIONARY);
        event_result.SetBoolKey("reload", false);

        base::PostTask(
            FROM_HERE,
            {
                content::BrowserThread::UI,
                base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
            },
            base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
                handler,
                std::move(event_name),
                std::move(event_result)
            )
        );
    }
}

void TransactionMobileService::db_request(std::unique_ptr<WalletHttpCallSignature> external_signature, int64_t current_height)
{
    std::string txid_from = db_helper_->get_last_confirmed_txid(current_height);

    db_request_internal(std::move(external_signature), txid_from);
}

void TransactionMobileService::db_request_internal(std::unique_ptr<WalletHttpCallSignature> external_signature, std::string txid_from)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (db_wallet_first_address_.empty())
    {
        return;
    }

    std::unique_ptr<WalletHttpCallSignature> transactions_signature = std::make_unique<WalletHttpCallSignature>(WalletHttpCallType::API_EXPLORER);
    transactions_signature->set_method_name("w/tx/list");

    transactions_signature->set_external_signature(std::move(external_signature));

    base::Value params(base::Value::Type::DICTIONARY);
    params.SetStringKey("address", db_wallet_first_address_);
    params.SetStringKey("from", txid_from);
    params.SetIntKey("count", MAX_TRANSACTIONS);

    transactions_signature->set_params(std::move(params));
    transactions_signature->set_qa(db_is_qa_);
    transactions_signature->append_extra_data("wallet_first_address", base::Value(db_wallet_first_address_));

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&TransactionMobileService::ui_transaction_request, base::Unretained(this), std::move(transactions_signature))
    );
}

void TransactionMobileService::ui_transaction_request(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    http_request->start(std::move(signature),
        base::BindOnce(&TransactionMobileService::ui_transactions_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void TransactionMobileService::ui_transactions_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto it = ui_requests_.find(http_request_ptr);
    if (it == ui_requests_.end())
    {
        LOG(WARNING) << "failed to find http_request, " << http_request_ptr;
    }
    else
    {
        std::unique_ptr<WalletRequest> http_request = std::move(it->second);
        ui_requests_.erase(it);
    }

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionMobileService::db_transactions_response, base::Unretained(this),
                         std::move(signature), std::move(results)));
}

bool get_string(const base::Value& value, std::string key, std::string& output)
{
    const std::string* s = value.FindStringKey(key);
    if (s)
    {
        output = std::string(*s);
        return true;
    }

    return false;
}

bool get_int(const base::Value& value, const std::string& key, int& output)
{
    base::Optional<int> s_int = value.FindIntKey(key);
    if (s_int != base::nullopt)
    {
        output = *s_int;
        return true;
    }

    return false;
}

bool get_amount(const base::Value& value, const std::string& key, int64_t& output)
{
    base::Optional<double> s_double = value.FindDoubleKey(key);
    if (s_double != base::nullopt)
    {
        output = rint(*s_double * 100000000);
        return true;
    }

    return false;
}

std::string get_category(int txtype)
{
    if (txtype & TXTYPE_POW)
    {
        return "pos";
    }

    if (txtype & TXTYPE_POS)
    {
        if (txtype & TXTYPE_STAKE)
        {
            if (txtype & TXTYPE_MASTERNODE)
            {
                return "stakemasternode";
            }
            else
            {
                return "pos";
            }
        }
        else if (txtype & TXTYPE_MASTERNODE)
        {
            return "masternode";
        }

        return "pos";
    }

    if (txtype & TXTYPE_SEND)
    {
        return "send";
    }

    if (txtype & TXTYPE_RECEIVE)
    {
        return "receive";
    }

    return "unknown";
}

void TransactionMobileService::db_transactions_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // if db_wallet_first_address_.changed?
    std::string* first_address = signature->get_extra_data().FindStringKey("wallet_first_address");

    if (!first_address || *first_address != db_wallet_first_address_)
    {
        return;
    }

    std::unique_ptr<WalletHttpCallSignature> external_signature = signature->get_external_signature();

    const base::Value* transactions_raw = results.FindListKey("data");
    if (transactions_raw && transactions_raw->is_list())
    {
        std::string last_txid ;

        sql::Transaction committer(db_helper_->get_db());
        committer.Begin();

        for(const base::Value& t : transactions_raw->GetList())
        {
            bool valid = true;

            TransactionData transaction_data;
            valid &= get_string(t,   "txid",     transaction_data.txid);
            valid &= !transaction_data.txid.empty();
            valid &= get_int(t,      "type",     transaction_data.txtype);
            valid &= get_amount(t,   "amount",   transaction_data.amount);

            transaction_data.category = get_category(transaction_data.txtype);

            if ("send" == transaction_data.category)
            {
                valid &= get_amount(t,   "fee",      transaction_data.fee);
                transaction_data.amount = transaction_data.amount + transaction_data.fee;
            }

            valid &= get_int(t,      "height",   transaction_data.height);
            valid &= get_int(t,      "time",     transaction_data.at);
            valid &= get_string(t,   "address",  transaction_data.address_to);
            get_string(t,   "from",     transaction_data.address_from);

            if (valid)
            {
                last_txid = transaction_data.txid;
                db_helper_->insert_or_update(transaction_data);
            }
        }

        committer.Commit();

        if (MAX_TRANSACTIONS ==  transactions_raw->GetList().size())
        {
            if (!last_txid.empty())
            {
                return db_request_internal(std::move(external_signature), last_txid);
            }
        }
    }

    if (external_signature.get())
    {
        std::string* block_hash = external_signature->get_extra_data().FindStringKey("block_hash");

        if (block_hash)
        {
            db_helper_->update_block_hash(*block_hash);
        }

        IWalletTabHandler* handler  = external_signature->get_ui_handler();
        std::string event_name      = external_signature->get_event_name();
        base::Value event_result(base::Value::Type::DICTIONARY);
        event_result.SetBoolKey("reload", true);

        base::PostTask(
            FROM_HERE,
            {
                content::BrowserThread::UI,
                base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
            },
            base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
                handler,
                std::move(event_name),
                std::move(event_result)
            )
        );
    }
}

void TransactionMobileService::ui_check_transactions(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                        base::BindOnce(&TransactionMobileService::db_check_transactions, base::Unretained(this),
                        std::move(signature)));
}

void TransactionMobileService::db_check_transactions(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!db_helper_->is_resync_allowed())
    {
        LOG(WARNING) << "db_check_transactions, resync is not allowed";
        return;
    }

    std::string* address = signature->get_params().FindStringKey("address");
    if (!address || *address != db_wallet_first_address_)
    {
        return;
    }

    base::Optional<double> explorer_balance_raw = signature->get_params().FindDoubleKey("balance");
    if (base::nullopt == explorer_balance_raw)
    {
        return;
    }

    int64_t explorer_balance = rint(*explorer_balance_raw * 100000000);
    int64_t db_balance = db_helper_->get_balance();

    if (db_balance == explorer_balance)
    {
        return;
    }

    LOG(WARNING) << "db_check_transactions, recreating database";

    db_helper_->remove_database(db_wallet_first_address_);
    db_helper_->create_database_if_not_exists(db_wallet_first_address_);

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::process_results_to_js_broadcast, base::Unretained(g_browser_process->wallet_manager()),
            "reset_last_block_hash",
            base::Value(base::Value::Type::DICTIONARY)
        )
    );
}

void TransactionMobileService::ui_remove_database(std::unique_ptr<WalletHttpCallSignature> signature)
{
    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionMobileService::db_remove_database, base::Unretained(this),
                         std::move(signature)));
}

void TransactionMobileService::db_remove_database(std::unique_ptr<WalletHttpCallSignature> signature)
{
    if (!db_wallet_first_address_.empty())
    {
        db_helper_->remove_database(db_wallet_first_address_);
    }

    g_browser_process->wallet_manager()->on_remove_database(std::move(signature));
}

void TransactionMobileService::ui_desync(std::unique_ptr<WalletHttpCallSignature> signature)
{
    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionMobileService::db_desync, base::Unretained(this),
                         std::move(signature)));
}

void TransactionMobileService::db_desync(std::unique_ptr<WalletHttpCallSignature> signature)
{
    db_helper_->desync();

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();
    base::Value results(base::Value::Type::DICTIONARY);

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
            handler,
            std::move(event_name),
            std::move(results)
        )
    );
}

void TransactionMobileService::ui_dump(std::unique_ptr<WalletHttpCallSignature> signature)
{
    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionMobileService::db_dump, base::Unretained(this),
                         std::move(signature)));
}

void TransactionMobileService::db_dump(std::unique_ptr<WalletHttpCallSignature> signature)
{
    base::Value transactions = db_helper_->dump();

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();

    base::Value results(base::Value::Type::DICTIONARY);
    results.SetKey("transactions", std::move(transactions));

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
            handler,
            std::move(event_name),
            std::move(results)
        )
    );
}


void TransactionMobileService::ui_get_transactions(std::unique_ptr<WalletHttpCallSignature> signature)
{
    task_runner_->PostTask(FROM_HERE,
            base::BindOnce(&TransactionMobileService::db_get_transactions, base::Unretained(this), std::move(signature)));
}

void TransactionMobileService::db_get_transactions(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::Value transactions(base::Value::Type::DICTIONARY);

    if (!db_wallet_first_address_.empty())
    {
        transactions = db_helper_->get_transactions(signature->get_params());
    }

    IWalletTabHandler* handler  = signature->get_ui_handler();
    std::string event_name      = signature->get_event_name();

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::process_results_to_js, base::Unretained(g_browser_process->wallet_manager()),
            handler,
            std::move(event_name),
            std::move(transactions)
        )
    );
}

}