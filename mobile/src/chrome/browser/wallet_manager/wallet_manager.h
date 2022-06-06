#ifndef CHROME_BROWSER_WALLET_MANAGER_WALLET_MANAGER_H_
#define CHROME_BROWSER_WALLET_MANAGER_WALLET_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/netbox/session_manager/session_manager.h"
#include "components/netboxglobal_call/wallet_request.h"
#include "components/netboxglobal_call/wallet_tab_handler.h"
#include "components/netboxglobal_call/wallet_http_call_signature.h"

#include "components/gcm_driver/instance_id/instance_id.h"

namespace Netboxglobal
{


class WalletManager
{
public:
    WalletManager();
    ~WalletManager();

    void start();
    void stop();

    void add_handler(IWalletTabHandler* handler);
    void remove_handler(IWalletTabHandler* handler);

    void set_session_state(int wallet_session_state_);
    void set_auth(bool is_auth);
    void notify_auth(bool is_auth);
    void set_guid(std::string guid);

    void request_local(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void request_server(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void request_explorer(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void on_remove_database(std::unique_ptr<WalletHttpCallSignature> &&signature);

    void process_results_to_js(IWalletTabHandler* handler, std::string event_name, base::Value results);
    void process_results_to_js_broadcast(std::string event_name, base::Value results);

    std::string get_machine_id(); // TODO, get_description
    bool is_emulator();
private:
    std::unordered_set<IWalletTabHandler*> handlers_;
    bool is_auth_   = false;

    std::string installation_guid_;
    std::string first_address_;
    std::string xpub_;

    int token_flags_ = 0;
    std::string token_lang_;
    std::string token_first_address_;

    int wallet_session_state_ = -1;
    bool is_qa_ = false;
    std::string referrer_;

    bool feature_export_ = false;

    std::unique_ptr<WalletSessionManager> session_controller_;
    std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    void reload_wallet();
    void reload_session_controller();

    void on_http_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr);

    base::Value local_is_auth(std::unique_ptr<WalletHttpCallSignature> &&);
    base::Value local_is_wallet(std::unique_ptr<WalletHttpCallSignature> &&);
    void local_set_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_get_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_get_wallet_seed(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_reset_wallet(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_debug(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_toggle(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_toggle_feature_download(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_get_address_index(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_set_address_index(std::unique_ptr<WalletHttpCallSignature> &&signature);
    base::Value local_share(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void local_import_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void local_check_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void local_get_last_block_hash(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void local_transactions(std::unique_ptr<WalletHttpCallSignature> &&signature);

    void set_notification_token(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void GetTokenCompleted(std::unique_ptr<WalletHttpCallSignature> &&signature, int flags, const std::string lang, const std::string& token, instance_id::InstanceID::Result result);
    void set_notification_token_server(std::unique_ptr<WalletHttpCallSignature> &&signature, std::string token, int flags, std::string lang);
    void set_notification_token_response(int flags, std::string lang, std::string first_address, std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr);

    // debug methods
    void local_desync(std::unique_ptr<WalletHttpCallSignature> &&signature);

    DISALLOW_COPY_AND_ASSIGN(WalletManager);
};

}

#endif