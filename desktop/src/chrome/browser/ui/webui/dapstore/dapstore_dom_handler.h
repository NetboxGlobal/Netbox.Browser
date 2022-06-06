#ifndef CHROME_BROWSER_UI_WEBUI_DAPSTORE_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DAPSTORE_DOM_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/wallet/wallet_dom_handler.h"
#include "content/public/browser/web_ui_message_handler.h"

class DapstoreDOMHandler : public WalletDOMHandler
{
public:
    DapstoreDOMHandler(content::WebUI* web_ui);
    ~DapstoreDOMHandler() override;

    // WebUIMessageHandler implementation.
    void RegisterMessages() override;

 private:

    base::WeakPtrFactory<DapstoreDOMHandler> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(DapstoreDOMHandler);
};

#endif