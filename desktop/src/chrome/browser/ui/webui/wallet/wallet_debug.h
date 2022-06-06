
#ifndef CHROME_BROWSER_UI_WEBUI_WALLET_DEBUG_H_
#define CHROME_BROWSER_UI_WEBUI_WALLET_DEBUG_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

class WalletDebugUI : public content::WebUIController {
public:
    explicit WalletDebugUI(content::WebUI* web_ui);
private:
    DISALLOW_COPY_AND_ASSIGN(WalletDebugUI);
};

#endif