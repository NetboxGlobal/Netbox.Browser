#ifndef CHROME_BROWSER_TRANSACTION_SERVICE_H_
#define CHROME_BROWSER_TRANSACTION_SERVICE_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/transaction_service/transaction_db_helper.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "chrome/browser/netbox/call/wallet_http_call_signature.h"
#include "chrome/browser/netbox/call/wallet_request.h"
#include "sql/database.h"

namespace Netboxglobal
{

class TransactionService
{
public:
    explicit TransactionService();
    ~TransactionService();

    void ui_stop();

    void ui_set_rpc_token(WalletSessionManager::DataState data_state, const WalletSessionManager::Data &data);
    void ui_set_first_address(std::string token);
    void ui_pre_start(base::FilePath profile_path);
    void ui_get_transactions(std::unique_ptr<WalletHttpCallSignature> request);
private:
    void db_set_path(base::FilePath profile_path);
    void db_set_first_address(std::string wallet_first_address);
    void db_set_token(std::string token);
    void schedule_transaction_request();
    void ui_start();
    void db_start(std::unique_ptr<WalletHttpCallSignature>);
    void ui_rpc_request(std::unique_ptr<WalletHttpCallSignature>);
    void ui_rpc_response(std::unique_ptr<WalletHttpCallSignature>, base::Value, WalletRequest*);
    void db_rpc_response(std::unique_ptr<WalletHttpCallSignature> request, base::Value);
    void db_get_transactions(std::unique_ptr<WalletHttpCallSignature>);
    void db_check_synced();
    void ui_mnsync_request(std::unique_ptr<WalletHttpCallSignature> signature);
    void db_mnsync_request(std::unique_ptr<WalletHttpCallSignature> signature);
    void ui_mnsync_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value result, WalletRequest* http_request_ptr);
    void db_check_control_sum();
    void ui_rpc_balance_request(std::unique_ptr<WalletHttpCallSignature> signature);
    void ui_rpc_balance_response(std::unique_ptr<WalletHttpCallSignature>, base::Value, WalletRequest*);
    void db_rpc_balance_response(std::unique_ptr<WalletHttpCallSignature>, base::Value);
    void db_close();

    scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // данные поля можно использовать только в db методах
    std::string db_wallet_first_address_;
    std::string db_token_base64_;
    bool db_in_rpc_call_ = false;
    bool db_loaded_ = false;
    int db_control_sum_check_failed_count_ = 0;

    std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    base::TimeDelta pool_request_delta_;

    std::unique_ptr<TransactionDBHelper> db_helper_;

    SEQUENCE_CHECKER(sequence_checker_);
};

}

#endif
