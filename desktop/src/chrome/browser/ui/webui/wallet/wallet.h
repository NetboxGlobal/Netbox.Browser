
#ifndef CHROME_BROWSER_UI_WEBUI_WALLET_H_
#define CHROME_BROWSER_UI_WEBUI_WALLET_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class WalletUI : public content::WebUIController {
 public:
  explicit WalletUI(content::WebUI* web_ui);
  static base::RefCountedMemory* GetFaviconResourceBytes(ui::ScaleFactor scale_factor);
 private:
  DISALLOW_COPY_AND_ASSIGN(WalletUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_UI_H_