#include "chrome/browser/ui/webui/wallet/wallet_dom_handler.h"
#include <sstream>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/netbox/wallet_manager/wallet_manager.h"

WalletDOMHandler::WalletDOMHandler(content::WebUI* web_ui)
{
    g_browser_process->wallet_manager()->add_handler(this);
}

WalletDOMHandler::~WalletDOMHandler()
{
    g_browser_process->wallet_manager()->remove_handler(this);
}

void WalletDOMHandler::OnJavascriptDisallowed()
{
}

void WalletDOMHandler::RenderProcessGone(base::TerminationStatus status)
{
}

void WalletDOMHandler::RegisterMessages()
{
    web_ui()->RegisterMessageCallback("environment",            base::BindRepeating(&WalletDOMHandler::HandleEnvironment,           weak_ptr_factory_.GetWeakPtr()));

    web_ui()->RegisterMessageCallback("universal",              base::BindRepeating(&WalletDOMHandler::HandleUniversal,             weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("wallet_raw",             base::BindRepeating(&WalletDOMHandler::HandleWalletRaw,             weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("universal_web",          base::BindRepeating(&WalletDOMHandler::HandleUniversalWeb,          weak_ptr_factory_.GetWeakPtr()));
	web_ui()->RegisterMessageCallback("universal_account",      base::BindRepeating(&WalletDOMHandler::HandleUniversalAccount,      weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("web_simple",             base::BindRepeating(&WalletDOMHandler::HandleWebSimple,             weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("web_url",                base::BindRepeating(&WalletDOMHandler::HandleWebUrl,                weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("service",                base::BindRepeating(&WalletDOMHandler::HandleService,               weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback("bridge",                 base::BindRepeating(&WalletDOMHandler::HandleBridge,                weak_ptr_factory_.GetWeakPtr()));
}

void WalletDOMHandler::HandleEnvironment(const base::ListValue* args)
{
    std::string event_name_raw = "environment";

    // try
    if (args)
    {
        args->GetString(0, &event_name_raw);
    }

    g_browser_process->wallet_manager()->environment(this, event_name_raw);
}


void WalletDOMHandler::HandleWalletRaw(const base::ListValue* args)
{
    std::string method_name_raw, event_name_raw, param_raw;

    if (!args->GetString(0, &method_name_raw)
    || !args->GetString(1, &event_name_raw)
    || !args->GetString(2, &param_raw))
    {
        base::DictionaryValue data;

        data.SetKey("error", base::Value(22));
        data.SetKey("errortext", base::Value("params not found"));

        base::Value event_name(event_name_raw);
        web_ui()->CallJavascriptFunctionUnsafe("wallet_send_event", std::move(event_name), std::move(data));

        return;
    }

	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::RPC_RAW));
	signature->set_method_name(method_name_raw);
	signature->set_event_name(event_name_raw);
	signature->set_params(base::Value(param_raw));

    Netboxglobal::IWalletTabHandler* handler = this;
    signature->set_ui_handler(handler);

	return g_browser_process->wallet_manager()->request_http(std::move(signature));
}

void WalletDOMHandler::HandleUniversal(const base::ListValue* args)
{
	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::RPC_JSON));
	HandleRequestInternal(std::move(signature), args);
}

void WalletDOMHandler::HandleUniversalWeb(const base::ListValue* args)
{
    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_SIGNED));
    HandleRequestInternal(std::move(signature), args);
}

void WalletDOMHandler::HandleUniversalAccount(const base::ListValue* args)
{
    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_ACCOUNT));
    HandleRequestInternal(std::move(signature), args);
}

void WalletDOMHandler::HandleWebSimple(const base::ListValue* args)
{
	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_SIGNED_SIMPLE));

    std::string method_name;
    args->GetString(0, &method_name);
	signature->set_method_name(method_name);


    std::string event_name = method_name;
    args->GetString(1, &event_name);
	signature->set_event_name(event_name);

    Netboxglobal::IWalletTabHandler* handler = this;
    signature->set_ui_handler(handler);

	return g_browser_process->wallet_manager()->request_http(std::move(signature));
}

void WalletDOMHandler::HandleWebUrl(const base::ListValue* args)
{
	std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::URL));

    std::string event_name;
    args->GetString(0, &event_name);
	signature->set_event_name(event_name);

    std::string url;
    args->GetString(1, &url);
	signature->set_method_name(url);

    Netboxglobal::IWalletTabHandler* handler = this;
    signature->set_ui_handler(handler);

	return g_browser_process->wallet_manager()->request_http(std::move(signature));
}

void WalletDOMHandler::HandleService(const base::ListValue* args)
{
    std::string method_name;
    args->GetString(0, &method_name);

    std::string event_name;
    args->GetString(1, &event_name);

    const base::Value* method_params;
    if (args->Get(2, &method_params) && method_params)
    {
        g_browser_process->wallet_manager()->service(this, method_name, event_name, method_params->Clone());
    }
}

void WalletDOMHandler::HandleBridge(const base::ListValue* args)
{
   std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_BRIDGE));
   HandleRequestInternal(std::move(signature), args);
}

void WalletDOMHandler::HandleRequestInternal(std::unique_ptr<Netboxglobal::WalletHttpCallSignature> &&signature, const base::ListValue* args)
{
    std::string method_name;
    if (args->GetString(0, &method_name))
    {
        signature->set_method_name(method_name);
    }

    int params_index = 1;

    std::string event_name;
    if (args->GetString(1, &event_name))
    {
        signature->set_event_name(event_name);
        params_index++;
    }
    else
    {
        signature->set_event_name(method_name);
    }

    const base::Value* params;
    if (args->Get(params_index, &params) && params)
    {
        signature->set_params(params->Clone());
    }

    Netboxglobal::IWalletTabHandler* handler = this;
    signature->set_ui_handler(handler);

    if (Netboxglobal::WalletHttpCallType::API_BRIDGE == signature->get_type()
		|| Netboxglobal::WalletHttpCallType::API_SIGNED == signature->get_type()
		|| Netboxglobal::WalletHttpCallType::API_ACCOUNT == signature->get_type()
		|| Netboxglobal::WalletHttpCallType::RPC_JSON == signature->get_type()
		)
    {
        return g_browser_process->wallet_manager()->request_http(std::move(signature));
    }

    LOG(WARNING) << "unhandled method";
}

void WalletDOMHandler::OnTabCall(std::string event_name_raw, base::Value result)
{
    base::Value event_name(event_name_raw);
    web_ui()->CallJavascriptEvent(std::move(event_name), std::move(result));
}