#include "chrome/browser/ui/webui/mobile_wallet/mobile_wallet_debug.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/mobile_wallet/mobile_wallet_dom_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"


#include "chrome/browser/ui/webui/components/components_handler.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

content::WebUIDataSource* CreateVersionUIDataDebugSource()
{
    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUIMobileWalletDebugHost);
    source->UseStringsJs();
    source->AddResourcePath("css/app.css",      IDR_WALLET_DEBUG_CSS);
    source->AddResourcePath("js/app.js",        IDR_WALLET_DEBUG_JS);

    source->SetDefaultResource(IDR_WALLET_DEBUG_HTML);

    return source;
}


MobileWalletDebugUI::MobileWalletDebugUI(content::WebUI* web_ui) : content::WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);
    web_ui->AddMessageHandler(std::make_unique<WalletMobileDOMHandler>());

    content::WebUIDataSource::Add(profile, CreateVersionUIDataDebugSource());
}

MobileWalletDebugUI::~MobileWalletDebugUI()
{
}

base::RefCountedMemory* MobileWalletDebugUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_PLUGINS_FAVICON, scale_factor);
}


