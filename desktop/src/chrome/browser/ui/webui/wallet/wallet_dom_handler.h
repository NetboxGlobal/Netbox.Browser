#ifndef CHROME_BROWSER_UI_WEBUI_WALLET_WALLET_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WALLET_WALLET_DOM_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/netbox/call/wallet_http_call_signature.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

#include <string>

namespace base
{
class ListValue;
}

namespace content {
class WebContents;
class WebUI;
}

class WalletDOMHandler : public virtual Netboxglobal::IWalletTabHandler,
                         public content::WebContentsObserver,
                         public content::WebUIMessageHandler {
public:
    WalletDOMHandler(content::WebUI* web_ui);
    ~WalletDOMHandler() override;

    // WebUIMessageHandler implementation.
    void RegisterMessages() override;
    void OnJavascriptDisallowed() override;

    // WebContentsObserver implementation.
    void RenderProcessGone(base::TerminationStatus status) override;

    // IWalletTabHandler implementation
    void OnTabCall(std::string event_name, base::Value result) override;
protected:

	void HandleEnvironment(const base::ListValue* args);
    void HandleWalletRaw(const base::ListValue* args);
    void HandleUniversal(const base::ListValue* args);
    void HandleUniversalWeb(const base::ListValue* args);
	void HandleUniversalAccount(const base::ListValue* args);
    void HandleWebSimple(const base::ListValue* args);
    void HandleWebUrl(const base::ListValue* args);
    void HandleService(const base::ListValue* args);
	void HandleBridge(const base::ListValue* args);

	void HandleRequestInternal(std::unique_ptr<Netboxglobal::WalletHttpCallSignature> &&signature, const base::ListValue* args);

private:
    base::WeakPtrFactory<WalletDOMHandler> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(WalletDOMHandler);
};

#endif
