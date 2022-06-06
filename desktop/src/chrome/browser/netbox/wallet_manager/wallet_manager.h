#ifndef CHROME_BROWSER_WALLET_MANAGER_H_
#define CHROME_BROWSER_WALLET_MANAGER_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "chrome/browser/netbox/call/wallet_http_call_signature.h"
#include "chrome/browser/netbox/call/wallet_tab_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/cookies/canonical_cookie.h"

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GURL;

namespace network
{
class SimpleURLLoader;
struct ResourceResponseHead;
}

namespace net
{
class URLRequestContextGetter;
}

namespace base
{
class Value;
}

namespace Netboxglobal
{

class WalletSessionManager;

enum WalletConfigureAction
{
    WCA_NONE   = 0,
    WCA_WAIT_AND_START,
    WCA_STOP_AND_START
};

class WalletManager
{
private:
    using FirstAddressEventObserversList = std::list<std::function<void(const std::string &)>>;
public:
    // struct AddressData
    // {
        // int32_t index = 0;
        // std::string address;
    // };
    //typedef base::Callback<void(float)> update_balance_callback;
    WalletManager();
    ~WalletManager();

    bool start();
    bool stop();

    void add_registrars();

    void add_handler(IWalletTabHandler* handler);
    void remove_handler(IWalletTabHandler* handler);

    void add_first_address_observer(FirstAddressEventObserversList::value_type val);

    // public JS functions
    void environment(IWalletTabHandler* handler, const std::string &event_name);
    void get_balance(IWalletTabHandler* handler);

    void service(IWalletTabHandler* handler, std::string method_name, std::string event_name, base::Value params);
	void request_explorer(std::unique_ptr<WalletHttpCallSignature> &&signature);

	void request_http(std::unique_ptr<WalletHttpCallSignature> &&signature);
    void end_http_call(IWalletTabHandler*, base::Value, std::string);

    DISALLOW_COPY_AND_ASSIGN(WalletManager);
private:
    // check functions
    void on_environment_ready(WalletSessionManager::DataState data_state, const WalletSessionManager::Data &data);

	void notify_auth();
	void process_results_to_js_broadcast(std::string event_name, base::Value results);

    //ping functions
    void initialize_rpc_first_address();

    void request_balance();
    void schedule_wallet_stop_and_start();
    void schedule_wallet_wait_and_start();

    void notify_first_addres_observers();
    void notify_status_changed();
    void notify_balance_changed(double new_balance);

    // end_rpc_functions_XXX
    void end_ping_first_address_rpc(const base::Value* json_data);
    void end_get_balance_rpc(const base::Value* json_data);

    //void set_new_wallet_status(WalletStatus new_val, WalletConfigureAction = WCA_NONE);


	void on_http_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value results, WalletRequest* http_request_ptr);

	std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    //update_balance_callback update_balance_callback_;
    base::RepeatingTimer polling_timer_;
    std::recursive_mutex calls_mutex_;
    std::unordered_set<IWalletTabHandler*> handlers_;

	bool is_loaded = false;
	int environment_error 	   = 0;
	std::string 	first_address_;

    int32_t ping_count_ = 0;
    FirstAddressEventObserversList first_address_observers_;

    std::mutex cache_mutex_;
    std::vector<std::string> universal_methods_white_list;

    int request_first_address_web_interval_sec_;
};

}

#endif
