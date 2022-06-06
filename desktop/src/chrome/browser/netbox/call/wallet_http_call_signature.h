#ifndef COMPONENTS_NETBOXGLOBAL_CALL_WALLET_HTTP_CALL_SIGNATURE_H_
#define COMPONENTS_NETBOXGLOBAL_CALL_WALLET_HTTP_CALL_SIGNATURE_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/netbox/call/wallet_tab_handler.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace Netboxglobal
{

enum WalletHttpCallType
{
	URL 		= 1,
    RPC_JSON,
    RPC_RAW, // for lottery
    API_SIGNED,
	API_SIGNED_SIMPLE,
    // API_COMMON, // without
    // WEB, // just request url
    MOBILE_LOCAL,
    API_EXPLORER,
    API_NOTIFICATION,
	API_BRIDGE,
	API_ACCOUNT
};

class WalletHttpCallSignature{
public:
    explicit WalletHttpCallSignature(WalletHttpCallType type);
    ~WalletHttpCallSignature();
    WalletHttpCallSignature(WalletHttpCallSignature&&);
    WalletHttpCallSignature& operator=(WalletHttpCallSignature&&);

    WalletHttpCallType get_type();

    void set_method_name(const std::string);
    std::string get_method_name();

    void set_event_name(const std::string);
    std::string get_event_name();

    void set_params(base::Value params);
    bool has_param(const std::string& param_name);
    void append_param(const std::string& param_name, base::Value param_value);
    base::Value& get_params();

    void set_rpc_token(const std::string);
    std::string get_rpc_token();

	void set_extra_data(base::Value extra_data);
    base::Value& get_extra_data();

    void set_qa(bool is_qa);

    bool is_valid_to_call();

    void set_ui_handler(IWalletTabHandler* handler);
    IWalletTabHandler* get_ui_handler();

    void set_external_signature(std::unique_ptr<WalletHttpCallSignature>);
    std::unique_ptr<WalletHttpCallSignature> get_external_signature();
    bool has_external_signature();

    std::unique_ptr<network::ResourceRequest> process_request_headers();
    void process_request_body(network::SimpleURLLoader* sender);
    base::Value post_process(base::Value value, int32_t http_code);

private:
    void InternalMove(WalletHttpCallSignature&& that);
    base::Value post_process_helper(base::Value dict, base::Value error_value);

    std::string clue_get_params(std::string exclude_name);

    WalletHttpCallType type_;
    std::string method_name_;
    std::string event_name_;
    base::Value params_;
    base::Value extra_data_;
    std::string rpc_token_;
    bool is_qa_ = false;
    IWalletTabHandler* tab_handler_ = nullptr;
    std::unique_ptr<WalletHttpCallSignature> external_request_;

    DISALLOW_COPY_AND_ASSIGN(WalletHttpCallSignature);
};

}
#endif