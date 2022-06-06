#include "chrome/browser/transaction_service/transaction_service.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/transaction_service/transaction_helper.h"
#include "chrome/browser/netbox/wallet_manager/wallet_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "sql/transaction.h"

namespace Netboxglobal
{

TransactionService::TransactionService()
{
    DETACH_FROM_SEQUENCE(sequence_checker_);

    db_helper_.reset(new TransactionDBHelper());
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

TransactionService::~TransactionService()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui_stop();
}

void TransactionService::ui_stop()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui_requests_.clear();
    task_runner_.reset();
    db_helper_.reset();
}

void TransactionService::ui_pre_start(base::FilePath profile_path)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_set_path, base::Unretained(this),
                         profile_path));
}

void TransactionService::db_set_path(base::FilePath db_path)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    db_helper_->set_db_path(db_path);
}

void TransactionService::ui_set_rpc_token(WalletSessionManager::DataState, const WalletSessionManager::Data &)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::string access_token_raw;
    g_browser_process->env_controller()->get_wallet_rpc_credentails(access_token_raw);

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_set_token, base::Unretained(this),
                         access_token_raw));
}

void TransactionService::db_set_token(std::string token)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    db_token_base64_ = token;

    std::unique_ptr<WalletHttpCallSignature> empty_request;
    db_start(std::move(empty_request));
}

void TransactionService::ui_set_first_address(std::string wallet_first_address)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_set_first_address, base::Unretained(this),
                         wallet_first_address));

}

void TransactionService::db_set_first_address(std::string wallet_first_address)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (db_wallet_first_address_ != wallet_first_address && !wallet_first_address.empty())
    {
        db_loaded_ = false;
        db_control_sum_check_failed_count_ = 0;

        if (!db_helper_->check_database(wallet_first_address, false))
        {
            VLOG(1) << "Failed to open database for " << wallet_first_address;
        }
    }

    db_wallet_first_address_ = wallet_first_address;


    std::unique_ptr<WalletHttpCallSignature> empty_request;
    db_start(std::move(empty_request));
}

void TransactionService::schedule_transaction_request()
{
    base::PostDelayedTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&TransactionService::ui_start, base::Unretained(this)),
        base::TimeDelta::FromSeconds(60)
    );
}

void TransactionService::ui_start()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<WalletHttpCallSignature> empty_request;
    task_runner_->PostTask(FROM_HERE,
                        base::BindOnce(&TransactionService::db_start, base::Unretained(this), std::move(empty_request)));
}

void TransactionService::db_start(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (db_wallet_first_address_.empty() || db_token_base64_.empty() || db_in_rpc_call_)
    {
        if (signature.get())
        {
            db_get_transactions(std::move(signature));
        }

        return;
    }

    db_in_rpc_call_ = true;

    std::unique_ptr<WalletHttpCallSignature> transactions_signature = std::make_unique<WalletHttpCallSignature>(WalletHttpCallType::RPC_JSON);

    std::string db_latest_block = db_helper_->get_latest_block();
    base::ListValue params;
    params.Append(db_latest_block);
    transactions_signature->set_params(std::move(params));

    transactions_signature->set_rpc_token(db_token_base64_);

    base::Value extra_data(base::Value::Type::DICTIONARY);
    extra_data.SetStringKey("wallet_first_address", db_wallet_first_address_);
    transactions_signature->set_extra_data(std::move(extra_data));

    if (signature.get())
    {
        transactions_signature->set_external_signature(std::move(signature));
    }

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&TransactionService::ui_rpc_request, base::Unretained(this), std::move(transactions_signature))
    );
}

void TransactionService::ui_rpc_request(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    signature->set_method_name("listsinceblock");

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    if (g_browser_process->env_controller()->is_qa())
    {
        signature->set_qa(true);
    }

    http_request->start(std::move(signature),
        base::BindOnce(&TransactionService::ui_rpc_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void TransactionService::ui_rpc_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto it = ui_requests_.find(http_request_ptr);
    if (it == ui_requests_.end())
    {
        VLOG(NETBOX_LOG_LEVEL) << "failed to find http_request, " << http_request_ptr;
    }
    else
    {
        std::unique_ptr<WalletRequest> http_request = std::move(it->second);
        ui_requests_.erase(it);
    }

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_rpc_response, base::Unretained(this),
                         std::move(signature), std::move(results)));
}

void TransactionService::db_rpc_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::string* first_address = signature->get_extra_data().FindStringKey("wallet_first_address");

    std::unique_ptr<WalletHttpCallSignature> external_signature = signature->get_external_signature();

    if (!first_address || *first_address != db_wallet_first_address_)
    {
        db_in_rpc_call_ = false;

        if (external_signature.get())
        {
            db_get_transactions(std::move(external_signature));
        }

        return;
    }

    std::map<std::string, TransactionData> rpc_data = parse_transactions_json(results);

    if (!rpc_data.empty() && db_helper_->is_open())
    {
		// transactions with confirmations < 101 & !conflicted
		std::map<std::string, TransactionData> db_data = db_helper_->get_unconfirmed(db_wallet_first_address_);

		std::string latest_block;

        sql::Transaction committer(db_helper_->get_db());
        committer.Begin();

        // rpc_data new ... old, so iterate in reverse order
		for(auto rpc_it = rpc_data.rbegin(); rpc_it != rpc_data.rend(); rpc_it++)
		{
			if (rpc_it->second.confirmations >= ENOUGH_CONFIRMATION_COUNT && !rpc_it->second.conflicted)
			{
				latest_block = rpc_it->second.blockhash;
			}

			auto db_it = db_data.find(rpc_it->second.key());
			if (db_it == db_data.end())
			{

                // do not insert empty from transaction
                if (rpc_it->second.address_from.empty()
                  && -1 == rpc_it->second.confirmations)
                {
                    for(auto db_it_with_empty_from = db_data.begin(); db_it_with_empty_from != db_data.end(); db_it_with_empty_from++)
                    {
                        if (db_it_with_empty_from->second.is_same_without_address_from(rpc_it->second))
                        {
                            db_data.erase(db_it_with_empty_from);
                            break;
                        }
                    }

                    continue;
                }

                db_helper_->insert_or_update(rpc_it->second);
                continue;
			}

			if (db_it->second.confirmations != rpc_it->second.confirmations ||
				db_it->second.conflicted != rpc_it->second.conflicted ||
                db_it->second.blocknumber != rpc_it->second.blocknumber ||
                db_it->second.blockhash != rpc_it->second.blockhash ||
                db_it->second.at != rpc_it->second.at
			)
			{
					db_helper_->update(rpc_it->second);
			}
			db_data.erase(db_it);
		}

		// mark record as conflicted
		for(auto db_conflicted_it = db_data.begin(); db_conflicted_it != db_data.end(); db_conflicted_it++)
		{
			db_helper_->set_conflicted(db_conflicted_it->second);
		}

		// move latest block
		if (!latest_block.empty())
		{
			db_helper_->set_latest_block(latest_block);
		}

        committer.Commit();
	}

    db_loaded_ = true;
    db_in_rpc_call_ = false;


    if (external_signature.get())
    {
        db_get_transactions(std::move(external_signature));
        return;
    }

    db_check_synced();
}

void TransactionService::db_check_synced()
{
    auto signature = std::make_unique<WalletHttpCallSignature>(WalletHttpCallType::RPC_JSON);
    signature->set_method_name("mnsync");
    signature->set_rpc_token(db_token_base64_);

    base::Value params(base::Value::Type::LIST);
    params.Append("status");
    signature->set_params(std::move(params));

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&TransactionService::ui_mnsync_request, base::Unretained(this), std::move(signature)));
}

void TransactionService::ui_mnsync_request(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (g_browser_process->env_controller()->is_qa())
    {
        signature->set_qa(true);
    }

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    http_request->start(std::move(signature),
        base::BindOnce(&TransactionService::ui_mnsync_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void TransactionService::ui_mnsync_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value result, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto it = ui_requests_.find(http_request_ptr);
    if (it == ui_requests_.end())
    {
        VLOG(NETBOX_LOG_LEVEL) << "failed to find http_request, " << http_request_ptr;
    }
    else
    {
        std::unique_ptr<WalletRequest> http_request = std::move(it->second);
        ui_requests_.erase(it);
    }

    if (result.is_dict())
    {
        absl::optional<bool> is_synced = result.FindBoolPath("result.IsBlockchainSynced");
        if (is_synced != absl::nullopt && true == *is_synced)
        {
            task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_check_control_sum, base::Unretained(this)));
            return;

        }
    }

    schedule_transaction_request();
}

void TransactionService::db_check_control_sum()
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto signature = std::make_unique<WalletHttpCallSignature>(WalletHttpCallType::RPC_JSON);
    signature->set_rpc_token(db_token_base64_);

    base::Value extra_data(base::Value::Type::DICTIONARY);
    extra_data.SetStringKey("wallet_first_address", db_wallet_first_address_);
    signature->set_extra_data(std::move(extra_data));

    signature->set_params(base::ListValue());

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&TransactionService::ui_rpc_balance_request, base::Unretained(this), std::move(signature)));
}

void TransactionService::ui_rpc_balance_request(std::unique_ptr<WalletHttpCallSignature> signature)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    signature->set_method_name("getbalance");
    if (g_browser_process->env_controller()->is_qa())
    {
        signature->set_qa(true);
    }

    std::unique_ptr<WalletRequest> http_request = std::make_unique<WalletRequest>();
    WalletRequest* http_request_ptr = http_request.get();

    http_request->start(std::move(signature),
        base::BindOnce(&TransactionService::ui_rpc_balance_response, base::Unretained(this)));

    ui_requests_[http_request_ptr] = std::move(http_request);
}

void TransactionService::ui_rpc_balance_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value result, WalletRequest* http_request_ptr)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto it = ui_requests_.find(http_request_ptr);
    if (it == ui_requests_.end())
    {
        VLOG(NETBOX_LOG_LEVEL) << "failed to find http_request, " << http_request_ptr;
    }
    else
    {
        std::unique_ptr<WalletRequest> http_request = std::move(it->second);
        ui_requests_.erase(it);
    }

    task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TransactionService::db_rpc_balance_response, base::Unretained(this),
                         std::move(signature), std::move(result)));
}

void TransactionService::db_rpc_balance_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results)
{
    std::string* first_address = signature->get_extra_data().FindStringKey("wallet_first_address");

    if (!first_address || *first_address != db_wallet_first_address_)
    {
        return;
    }

    int64_t db_balance = db_helper_->get_balance();

    if (!results.is_dict())
    {
		schedule_transaction_request();
		return;
	}

	absl::optional<double> rpc_balance_raw = results.FindDoubleKey("result");
	if (rpc_balance_raw == absl::nullopt)
	{
		schedule_transaction_request();
		return;
	}

	int64_t rpc_balance = rint(*rpc_balance_raw * 100000000);

    if (db_balance == rpc_balance)
    {
        db_control_sum_check_failed_count_ = 0;
    }
    else
    {
        db_control_sum_check_failed_count_++;
        VLOG(NETBOX_LOG_LEVEL) << "balances mistmatch";
    }

    if (db_control_sum_check_failed_count_ > 15)
    {
        db_loaded_ = false;
        VLOG(1) << "balances mistmatch, recreating database";
        db_helper_->check_database(db_wallet_first_address_, true);
    }

    schedule_transaction_request();
}

void TransactionService::ui_get_transactions(std::unique_ptr<WalletHttpCallSignature> request)
{
    if (absl::nullopt == request->get_params().FindBoolKey("invalidate"))
    {
        task_runner_->PostTask(FROM_HERE,
            base::BindOnce(&TransactionService::db_get_transactions, base::Unretained(this), std::move(request)));
        return;
    }

    task_runner_->PostTask(FROM_HERE,
            base::BindOnce(&TransactionService::db_start, base::Unretained(this), std::move(request)));
}

void TransactionService::db_get_transactions(std::unique_ptr<WalletHttpCallSignature> request)
{
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::Value transactions = db_helper_->get_transactions(request->get_params());

    std::string event_name          = request->get_event_name();
    IWalletTabHandler* handler      = request->get_ui_handler();

    if (false == db_loaded_)
    {
        transactions.SetBoolKey("is_loading", true);
    }
    else
    {
        transactions.SetBoolKey("is_loading", false);
    }

    base::PostTask(
        FROM_HERE,
        {
            content::BrowserThread::UI,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
        },
        base::BindOnce(&WalletManager::end_http_call, base::Unretained(g_browser_process->wallet_manager()), handler, std::move(transactions), std::move(event_name))
    );
}

}