#include "chrome/browser/ui/webui/wallet/wallet_debug.h"

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


WalletDebugUI::WalletDebugUI(content::WebUI* web_ui) : WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<WalletDOMHandler>(web_ui));

    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUIWalletDebugHost);

    source->AddResourcePath("debug.js",                    IDR_DEBUG_JS);

    source->DisableTrustedTypesCSP();

    source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://wallet 'self' 'unsafe-eval' "
        "'unsafe-inline';");

    source->SetDefaultResource(IDR_DEBUG_HTML);

    content::WebUIDataSource::Add(profile, source);
    content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

}