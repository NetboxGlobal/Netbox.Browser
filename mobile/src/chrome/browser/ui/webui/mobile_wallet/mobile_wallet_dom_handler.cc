#include "chrome/browser/ui/webui/mobile_wallet/mobile_wallet_dom_handler.h"

#include "base/sequence_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/wallet_manager/wallet_manager.h"
#include "content/public/browser/browser_thread.h"

WalletMobileDOMHandler::WalletMobileDOMHandler()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    g_browser_process->wallet_manager()->add_handler(this);
}

WalletMobileDOMHandler::~WalletMobileDOMHandler()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    g_browser_process->wallet_manager()->remove_handler(this);
}

void WalletMobileDOMHandler::RegisterMessages()
{
    web_ui()->RegisterMessageCallback(
        "local",
        base::BindRepeating(&WalletMobileDOMHandler::HandleRequestLocal,base::Unretained(this)));

    web_ui()->RegisterMessageCallback(
        "server",
        base::BindRepeating(&WalletMobileDOMHandler::HandleRequestServer,base::Unretained(this)));

    web_ui()->RegisterMessageCallback(
        "explorer",
        base::BindRepeating(&WalletMobileDOMHandler::HandleRequestExplorer,base::Unretained(this)));

    web_ui()->RegisterMessageCallback(
        "bridge",
        base::BindRepeating(&WalletMobileDOMHandler::HandleRequestBridge,base::Unretained(this)));
}

void WalletMobileDOMHandler::HandleRequestLocal(const base::ListValue* args)
{
    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::MOBILE_LOCAL));
    HandleRequestInternal(std::move(signature), args);
}

void WalletMobileDOMHandler::HandleRequestServer(const base::ListValue* args)
{
    std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_SIGNED));
    HandleRequestInternal(std::move(signature), args);
}

void WalletMobileDOMHandler::HandleRequestExplorer(const base::ListValue* args)
{
   std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_EXPLORER));

   HandleRequestInternal(std::move(signature), args);
}

void WalletMobileDOMHandler::HandleRequestBridge(const base::ListValue* args)
{
   std::unique_ptr<Netboxglobal::WalletHttpCallSignature> signature(new Netboxglobal::WalletHttpCallSignature(Netboxglobal::WalletHttpCallType::API_BRIDGE));

   HandleRequestInternal(std::move(signature), args);
}

void WalletMobileDOMHandler::HandleRequestInternal(std::unique_ptr<Netboxglobal::WalletHttpCallSignature> &&signature, const base::ListValue* args)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
    if (args->Get(params_index, &params) && params && params->is_dict())
    {
        signature->set_params(params->Clone());
    }

    Netboxglobal::IWalletTabHandler* handler = this;
    signature->set_ui_handler(handler);

    if (Netboxglobal::WalletHttpCallType::MOBILE_LOCAL == signature->get_type())
    {
        return g_browser_process->wallet_manager()->request_local(std::move(signature));
    }
    else if (Netboxglobal::WalletHttpCallType::API_SIGNED == signature->get_type())
    {
        return g_browser_process->wallet_manager()->request_server(std::move(signature));
    }
    else if (Netboxglobal::WalletHttpCallType::API_EXPLORER == signature->get_type() || Netboxglobal::WalletHttpCallType::API_BRIDGE == signature->get_type())
    {
        return g_browser_process->wallet_manager()->request_explorer(std::move(signature));
    }

    base::Value event_name_error(event_name);
    base::Value result(base::Value::Type::DICTIONARY);
    result.SetKey("error", base::Value("Internal unhandled method"));

    web_ui()->CallJavascriptEvent(std::move(event_name_error), std::move(result));
}

void WalletMobileDOMHandler::OnTabCall(std::string event_name_raw, base::Value result)
{
    base::Value event_name(event_name_raw);
    web_ui()->CallJavascriptEvent(std::move(event_name), std::move(result));
}
