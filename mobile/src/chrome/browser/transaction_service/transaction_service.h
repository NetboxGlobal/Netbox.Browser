#ifndef CHROME_BROWSER_TRANSACTION_SERVICE_H_
#define CHROME_BROWSER_TRANSACTION_SERVICE_H_

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/transaction_service/transaction_db_helper.h"
#include "components/netboxglobal_call/wallet_http_call_signature.h"
#include "components/netboxglobal_call/wallet_request.h"
#include "sql/database.h"

namespace Netboxglobal
{

class TransactionMobileService
{
public:
    explicit TransactionMobileService();
    ~TransactionMobileService();

    void ui_pre_start(base::FilePath profile_path);
    void ui_stop();

    // init methods
    void ui_set_first_address(std::string wallet_first_address, bool is_qa);

    // work methods
    void ui_get_transactions(std::unique_ptr<WalletHttpCallSignature> request);
    void ui_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> request);
    void ui_import_transactions(std::unique_ptr<WalletHttpCallSignature> request, int current_height, std::string block_hash);
    void ui_check_transactions(std::unique_ptr<WalletHttpCallSignature> request);
    void ui_remove_database(std::unique_ptr<WalletHttpCallSignature> signature);

    // debug methods
    void ui_desync(std::unique_ptr<WalletHttpCallSignature> signature);
    void ui_dump(std::unique_ptr<WalletHttpCallSignature> signature);

private:

    scoped_refptr<base::SequencedTaskRunner> task_runner_;

    //
    void db_set_first_address(std::string wallet_first_address, bool is_qa);
    void db_set_path(base::FilePath db_path);
    void check_block_hash(std::unique_ptr<WalletHttpCallSignature> external_signature, int current_height, std::string block_hash);
    void db_request(std::unique_ptr<WalletHttpCallSignature> external_signature, int64_t current_height);
    void db_request_internal(std::unique_ptr<WalletHttpCallSignature> external_signature, std::string txid_from);
    void ui_transaction_request(std::unique_ptr<WalletHttpCallSignature> signature);
    void ui_transactions_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr);
    void db_transactions_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results);
    void db_check_transactions(std::unique_ptr<WalletHttpCallSignature> request);
    void db_remove_database(std::unique_ptr<WalletHttpCallSignature> signature);
    void db_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> external_signature);

    void db_get_transactions(std::unique_ptr<WalletHttpCallSignature> signature);

    // debug methods

    void db_desync(std::unique_ptr<WalletHttpCallSignature> signature);
    void db_dump(std::unique_ptr<WalletHttpCallSignature> signature);


    // данные поля можно использовать только в db методах
    std::string db_wallet_first_address_;
    bool db_is_qa_ = false;

    std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    std::unique_ptr<TransactionDBHelper> db_helper_;

    SEQUENCE_CHECKER(sequence_checker_);
    DISALLOW_COPY_AND_ASSIGN(TransactionMobileService);
};

}

#endif
