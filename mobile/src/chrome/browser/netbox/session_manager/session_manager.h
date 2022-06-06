#ifndef CHROME_BROWSER_NETBOX_SESSION_MANAGER_SESSION_MANAGER_H
#define CHROME_BROWSER_NETBOX_SESSION_MANAGER_SESSION_MANAGER_H

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/netboxglobal_call/wallet_request.h"
#include "components/netboxglobal_call/wallet_http_call_signature.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace Netboxglobal
{

#define WALLET_SESSION_LOADING -1
#define WALLET_SESSION_OK 0

class WalletSessionManager : public network::mojom::CookieChangeListener
{
public:
    explicit WalletSessionManager(bool is_qa);
    ~WalletSessionManager() override;

    void start(std::string referrer);
    void stop();

    std::string get_installation_guid();
    std::string get_machine_id();
    std::string get_session_guid();

    bool is_emulator();

private:
    void set_state(int state);
    std::string get_cookie_token_name() const;

    std::string guid_;
    std::string guid_checksum_;

    std::string machine_id_;
    bool is_emulator_ = false;
    std::string session_;
    void obtain_information();

    std::string referrer_;

    int guid_request_interval_seconds_ = 1;

    bool is_qa_ = false;
    bool is_first_launch_ = false; // TODO think over

    std::map<WalletRequest*, std::unique_ptr<WalletRequest>> ui_requests_;

    network::mojom::CookieManager* get_cookie_manager();

    void set_session_cookie();
    void on_set_session_cookie(net::CookieAccessResult set_cookie_result);
    void process_guid();
    void on_get_guid_from_cookie(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&);
    bool read_guid_from_cookies(const net::CookieAccessResultList &cookies);
    void send_new_guid_request();
    void on_guid_request_response(std::unique_ptr<WalletHttpCallSignature> signature, base::Value data, WalletRequest* http_request_ptr);
    //void process_send_new_guid_request(const base::string16 &new_guid, const base::string16 &new_guid_checksum_str);
    //void set_new_guid_data(const std::string &guid, const std::string &guid_sign);
    void set_data_to_cookie(const std::string &variable,
                                        const std::string &value,
                                        network::mojom::CookieManager::SetCanonicalCookieCallback &&callback);
    void on_set_guid_cookie_result(net::CookieAccessResult set_cookie_result);
    void on_set_guid_sign_cookie_result(net::CookieAccessResult set_cookie_result);
    void io_get_auth_cookie();
    void on_get_auth_cookies_list(const net::CookieAccessResultList& cookies, const net::CookieAccessResultList&);
    void create_auth_cookie_subscription();
    void OnCookieChange(const net::CookieChangeInfo& change) override;

    mojo::Receiver<network::mojom::CookieChangeListener> cookie_listener_binding_{this};

    DISALLOW_COPY_AND_ASSIGN(WalletSessionManager);
};

}

#endif