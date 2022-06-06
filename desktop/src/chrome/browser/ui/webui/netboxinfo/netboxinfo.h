
#ifndef CHROME_BROWSER_UI_WEBUI_NETBOXINFO_H_
#define CHROME_BROWSER_UI_WEBUI_NETBOXINFO_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class NetboxinfoUI : public content::WebUIController {
 public:
  explicit NetboxinfoUI(content::WebUI* web_ui);
  static base::RefCountedMemory* GetFaviconResourceBytes(ui::ScaleFactor scale_factor);
 private:
  DISALLOW_COPY_AND_ASSIGN(NetboxinfoUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NETBOXINFO_H_