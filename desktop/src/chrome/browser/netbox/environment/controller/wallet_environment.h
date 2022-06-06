#ifndef COMPONENTS_NETBOXGLOBAL_ENV_CONTROLLER_H_
#define COMPONENTS_NETBOXGLOBAL_ENV_CONTROLLER_H_

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/system/sys_info.h"
#include "chrome/browser/netbox/call/wallet_request.h"
#include "chrome/browser/netbox/call/wallet_http_call_signature.h"
#include "chrome/browser/netbox/environment/launch/wallet_launch.h"
#include "components/netboxglobal_hardware/hardware.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"


namespace network
{
    class SimpleURLLoader;
}

namespace net
{
    class URLRequestContextGetter;
}

namespace Netboxglobal
{

class WalletSessionManager : public network::mojom::CookieChangeListener
{
public:

	enum AuthState
	{
		AUTH_UNDEFINED = 0,
		AUTH_NOT_LOGGED = 1,
		AUTH_LOGGED = 2
	};

    enum DataState
    {
		
        DS_OK									= 0,		
        DS_NONE									= 50,
        DS_AUTH_COOKIE_ERROR					= 102,		
        DS_GUID_ERROR							= 104,
        DS_SESSION_ERROR						= 107,
		DS_WALLET_RPC_TOKEN_ERROR				= 109,
        DS_WALLET_DIR_NOT_FOUND					= 113,
        DS_WALLET_FILE_NOT_FOUND				= 114,
        DS_WALLET_PROCESS_NOT_FOUND				= 115,
        DS_WALLET_PROCESS_STARTED_WITH_ERROR 	= 116,
        DS_WALLET_PROCESS_STOPPED_WITH_ERROR 	= 117,
        
    };
    struct Data
    {
        std::string guid;
        std::string guid_checksum;
        std::string machine_id;

        Data();
        ~Data();
    };

private:
    using EventObserversList = std::list<std::function<void(DataState, const Data &)>>;
    using WalletRestartEventObserversList = std::list<std::function<void()>>;
public:
    WalletSessionManager();
    ~WalletSessionManager() override;
    void start();
    void stop();

    const std::string &get_guid() const;
    const std::string &get_encrypted_machine_id() const;
    const std::string &get_wallet_exe_creation_time() const;
    const ExtendedHardwareInfo &get_hardware_info() const;

    bool is_qa() const;
    bool is_debug() const;
    bool is_first_launch() const;
	AuthState get_auth_state() const;

    void add_data_observer(EventObserversList::value_type val) const;

    std::vector<std::string> encrypt_data(const std::string &data) const;

    std::string create_api_request(const std::string &data) const;

    std::string get_api_url() const;
    std::string get_rpc_url() const;
    std::string get_auth_url() const;
    std::string get_cookie_url() const;
    std::string get_cookie_token_name() const;

    void get_wallet_rpc_credentails(std::string &wallet_rpc_token);
    void on_wallet_start(DataState state, std::string token, std::string wallet_exe_creation_time);

    void on_guid_request_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value data, WalletRequest* http_request_ptr);

    void wallet_start(WALLET_LAUNCH mode);

    void send_new_guid_request();

    DISALLOW_COPY_AND_ASSIGN(WalletSessionManager);

private:
    // helper functions
    network::mojom::CookieManager* get_cookie_manager();
    std::unique_ptr<net::CanonicalCookie> get_cookie_helper(const std::string& url, const std::string& domain, const std::string& name, const std::string& value);

    // state functions
    void change_auth_state(AuthState auth_state);
    void change_init_state(DataState data_state);
    void change_wallet_state(DataState data_state);

    void notify_state_observers();

    //IO thread tasks
    void io_get_auth_cookie();

    //UI thread tasks
    void set_session_cookie();

    void create_auth_cookie_subscription();

    void OnCookieChange(const net::CookieChangeInfo& change) override;

    // on_callback handlers
    void on_set_guid_cookie_result(net::CookieAccessResult set_cookie_result);
    void on_set_guid_sign_cookie_result(net::CookieAccessResult set_cookie_result);
    void on_set_session_cookie(net::CookieAccessResult set_cookie_result);
    void on_get_auth_cookies_list(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&);
    void on_get_guid_from_cookie(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&);
    bool read_guid_from_cookies(const net::CookieAccessResultList &cookies);

    void set_data_to_cookie(const std::string &variable,
                            const std::string &value,
                            network::mojom::CookieManager::SetCanonicalCookieCallback &&callback);
    void set_new_guid_data(const std::string &guid, const std::string &guid_sign);
    bool process_guid_data(const net::CookieAccessResultList &cookies, std::string &guid, std::string &guid_sign);

    void on_get_hardware_info(ExtendedHardwareInfo info);

    void process_guid();
    std::u16string get_promo_code();

    mojo::Receiver<network::mojom::CookieChangeListener> cookie_listener_binding_{this};

    std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    Data data_;
    DataState init_state_ = DS_NONE;
    DataState wallet_state_ = DS_NONE;
    AuthState auth_state_ = AUTH_UNDEFINED;
    bool is_qa_;
    bool is_debug_;
    bool is_first_launch_ = false;
    std::atomic<bool> wallet_restarting_;
    mutable EventObserversList data_observers_list_;
    mutable WalletRestartEventObserversList wallet_restart_observers_list_;
    std::mutex wallet_rpc_token_mutex_;
    std::string token_base64_;
    std::string encrypted_machine_id_;
    std::string wallet_exe_creation_time_;
    int guid_request_interval_seconds_ = 1;
    // hardware params
    ExtendedHardwareInfo hardware_info_;
};

}

#endif