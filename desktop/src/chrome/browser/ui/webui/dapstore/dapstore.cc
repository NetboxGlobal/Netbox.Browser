#include "chrome/browser/ui/webui/dapstore/dapstore.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "base/path_service.h"
#include "base/files/file_util.h"
#include "base/cpu.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/dapstore/dapstore_dom_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/grit/netbox_resources.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "components/netboxglobal_utils/wallet_utils.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"

base::RefCountedMemory* DapstoreUI::GetFaviconResourceBytes(ui::ScaleFactor scale_factor)
{
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(IDR_TAB_STORE_LOGO_16, scale_factor);
}

DapstoreUI::DapstoreUI(content::WebUI* web_ui) : WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<DapstoreDOMHandler>(web_ui));

    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUIDapstoreHost);

    source->AddResourcePath("js/app.js", IDR_DAPSTORE_JS);

    source->AddResourcePath("css/app.css", IDR_DAPSTORE_CSS);

    source->AddResourcePath("fonts/Montserrat-Medium.ttf",      IDR_WALLET_FONT_MONTSERRAT_MEDIUM);
    source->AddResourcePath("fonts/Montserrat-SemiBold.ttf",    IDR_WALLET_FONT_MONTSERRAT_SEMIBOLD);
    source->AddResourcePath("fonts/Montserrat-ExtraBold.ttf",   IDR_WALLET_FONT_MONTSERRAT_EXTRABOLD);
    source->AddResourcePath("fonts/Ubuntu-Regular.ttf", IDR_WALLET_FONT_UBUNTU_REGULAR);

    source->AddResourcePath("img/1st.svg", IDR_DAPSTORE_IMG_1);
    source->AddResourcePath("img/2nd.svg", IDR_DAPSTORE_IMG_2);
    source->AddResourcePath("img/3rd.svg", IDR_DAPSTORE_IMG_3);
    source->AddResourcePath("img/appStore.svg", IDR_DAPSTORE_IMG_APPSTORE);
    source->AddResourcePath("img/delete.svg", IDR_DAPSTORE_IMG_DELETE);
    source->AddResourcePath("img/icons.svg", IDR_DAPSTORE_IMG_ICONS);
    source->AddResourcePath("img/loading-logo.svg", IDR_DAPSTORE_IMG_LOADINGLOGO);
    source->AddResourcePath("img/logo.svg", IDR_DAPSTORE_IMG_LOGO);
    source->AddResourcePath("img/playMarket.svg", IDR_DAPSTORE_IMG_PLAYMARKET);
    source->AddResourcePath("img/slider-arrow.svg", IDR_DAPSTORE_IMG_SLIDERARROW);

    source->DisableTrustedTypesCSP();

    source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://wallet 'self' 'unsafe-eval' "
        "'unsafe-inline';");

    source->SetDefaultResource(IDR_DAPSTORE_HTML_MAIN);

    content::WebUIDataSource::Add(profile, source);
    content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}