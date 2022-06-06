#include "chrome/browser/ui/webui/wallet/wallet.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ui/webui/wallet/wallet_dom_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/netbox_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;
using content::WebContents;

base::RefCountedMemory* WalletUI::GetFaviconResourceBytes(ui::ScaleFactor scale_factor)
{
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(IDR_TAB_WALLET_LOGO_16, scale_factor);
}

WalletUI::WalletUI(content::WebUI* web_ui) : WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<WalletDOMHandler>(web_ui));

    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUIWalletHost);

    source->AddResourcePath("js/app.js",                    IDR_WALLET_JS);
    source->AddResourcePath("css/app.css",                  IDR_WALLET_CSS);


    source->AddResourcePath("img/appStore.svg",         IDR_WALLET_IMG_APPSTORE);
    source->AddResourcePath("img/delete.svg",           IDR_WALLET_IMG_DELETE);
    source->AddResourcePath("img/icons.svg",            IDR_WALLET_IMG_ICONS);
    source->AddResourcePath("img/loading-logo.svg",     IDR_WALLET_IMG_LOADINGLOGO);
    source->AddResourcePath("img/logo.svg",             IDR_WALLET_IMG_LOGO);
    source->AddResourcePath("img/playMarket.svg",       IDR_WALLET_IMG_PLAYMARKET);
    source->AddResourcePath("img/slider-arrow.svg",     IDR_WALLET_IMG_SLIDEARROW);

    source->AddResourcePath("fonts/Montserrat-Medium.ttf",      IDR_WALLET_FONT_MONTSERRAT_MEDIUM);
    source->AddResourcePath("fonts/Montserrat-SemiBold.ttf",    IDR_WALLET_FONT_MONTSERRAT_SEMIBOLD);
    source->AddResourcePath("fonts/Montserrat-ExtraBold.ttf",   IDR_WALLET_FONT_MONTSERRAT_EXTRABOLD);
    source->AddResourcePath("fonts/Ubuntu-Regular.ttf",         IDR_WALLET_FONT_UBUNTU_REGULAR);

    source->DisableTrustedTypesCSP();

    source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://wallet 'self' 'unsafe-eval' "
        "'unsafe-inline';");

    source->SetDefaultResource(IDR_WALLET_HTML_MAIN);

    content::WebUIDataSource::Add(profile, source);
    content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

}