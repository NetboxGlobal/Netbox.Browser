#ifndef CHROME_BROWSER_UI_WEBUI_DAPSTORE_H_
#define CHROME_BROWSER_UI_WEBUI_DAPSTORE_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

class DapstoreUI : public content::WebUIController {
 public:
  explicit DapstoreUI(content::WebUI* web_ui);
  static base::RefCountedMemory* GetFaviconResourceBytes(ui::ScaleFactor scale_factor);
 private:
  DISALLOW_COPY_AND_ASSIGN(DapstoreUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NETBOXINFO_H_