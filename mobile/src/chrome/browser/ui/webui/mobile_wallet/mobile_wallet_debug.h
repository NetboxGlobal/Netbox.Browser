#ifndef CHROME_BROWSER_UI_WEBUI_MOBILE_WALLET_DEBUG_H_
#define CHROME_BROWSER_UI_WEBUI_MOBILE_WALLET_DEBUG_H_

#include <memory>

#include <base/debug/stack_trace.h>
#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class MobileWalletDebugUI : public content::WebUIController {
public:
    explicit MobileWalletDebugUI(content::WebUI* web_ui);
    ~MobileWalletDebugUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

private:
  DISALLOW_COPY_AND_ASSIGN(MobileWalletDebugUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MOBILE_WALLET_DEBUG_H_