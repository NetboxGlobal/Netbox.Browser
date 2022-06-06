
#ifndef CHROME_BROWSER_UI_WEBUI_NEWS_H_
#define CHROME_BROWSER_UI_WEBUI_NEWS_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

class NewsUI : public content::WebUIController {
 public:
  explicit NewsUI(content::WebUI* web_ui);
 private:
  DISALLOW_COPY_AND_ASSIGN(NewsUI);
};

#endif