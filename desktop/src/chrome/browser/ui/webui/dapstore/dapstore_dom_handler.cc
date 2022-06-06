
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/dapstore/dapstore_dom_handler.h"
#include "chrome/browser/netbox/wallet_manager/wallet_manager.h"
#include "base/bind.h"

DapstoreDOMHandler::DapstoreDOMHandler(content::WebUI* web_ui)
    : WalletDOMHandler(web_ui)
{
    g_browser_process->wallet_manager()->add_handler(this);
}

DapstoreDOMHandler::~DapstoreDOMHandler()
{
    g_browser_process->wallet_manager()->remove_handler(this);
}

void DapstoreDOMHandler::RegisterMessages()
{
    WalletDOMHandler::RegisterMessages();
}