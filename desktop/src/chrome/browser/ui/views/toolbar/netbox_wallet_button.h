#ifndef CHROME_BROWSER_UI_VIEWS_NETBOX_WALLET_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_NETBOX_WALLET_BUTTON_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/button.h"

class Browser;
class ToolbarView;

class NetboxWalletButton : public views::LabelButton, public content::NotificationObserver {
public:
  NetboxWalletButton(PressedCallback callback, const std::u16string& initial_text);
  ~NetboxWalletButton() override;

  //std::unique_ptr<views::InkDrop> CreateInkDrop() override;  
  //std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight() const override;
  //SkColor GetInkDropBaseColor() const override;
  void add_registrars();
  void OnThemeChanged() override;    
private: 
	int32_t status_ = 0;
	  
  const char* GetClassName() const override;

  content::NotificationRegistrar registrar_;  
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  
  DISALLOW_COPY_AND_ASSIGN(NetboxWalletButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NETBOX_WALLET_BUTTON_H_