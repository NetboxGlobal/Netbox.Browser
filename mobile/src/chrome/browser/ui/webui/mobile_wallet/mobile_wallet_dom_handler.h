#ifndef CHROME_BROWSER_UI_WEBUI_MOBILE_WALLET_MOBILE_WALLET_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MOBILE_WALLET_MOBILE_WALLET_DOM_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "components/netboxglobal_call/wallet_http_call_signature.h"
#include "components/netboxglobal_call/wallet_tab_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebContents;
class WebUI;
}

class WalletMobileDOMHandler :
                        public virtual Netboxglobal::IWalletTabHandler,
                        public content::WebUIMessageHandler {
public:
    WalletMobileDOMHandler();
    ~WalletMobileDOMHandler() override;

    // WebUIMessageHandler implementation.
    void RegisterMessages() override;

    void OnTabCall(std::string event_name_raw, base::Value result) override;

private:
    base::WeakPtrFactory<WalletMobileDOMHandler> weak_ptr_factory_{this};

    void HandleRequest(const base::ListValue* args);
    void HandleRequestLocal(const base::ListValue* args);
    void HandleRequestServer(const base::ListValue* args);
    void HandleRequestExplorer(const base::ListValue* args);
    void HandleRequestBridge(const base::ListValue* args);
    void HandleRequestInternal(std::unique_ptr<Netboxglobal::WalletHttpCallSignature> &&signature, const base::ListValue* args);

    DISALLOW_COPY_AND_ASSIGN(WalletMobileDOMHandler);
};

#endif
