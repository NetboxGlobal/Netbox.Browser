#include "chrome/browser/ui/webui/mobile_wallet/mobile_wallet.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/components/components_handler.h"
#include "chrome/browser/ui/webui/mobile_wallet/mobile_wallet_dom_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

#include "ui/gfx/paint_vector_icon.h"

content::WebUIDataSource* CreateVersionUIDataSource()
{
    content::WebUIDataSource* source = content::WebUIDataSource::Create(chrome::kChromeUIMobileWalletHost);
    source->UseStringsJs();
    source->AddResourcePath("js/app.js",        IDR_WALLET_JS);
    source->AddResourcePath("css/app.css",      IDR_WALLET_CSS);
    source->AddResourcePath("img/arrow.svg",    IDR_WALLET_IMG_ARROW);
    source->AddResourcePath("img/blackout.png", IDR_WALLET_IMG_BLACKOUT);
    source->AddResourcePath("img/check.png",    IDR_WALLET_IMG_CHECK);
    source->AddResourcePath("img/failure.png",  IDR_WALLET_IMG_FAILURE);
    source->AddResourcePath("img/frame.png",    IDR_WALLET_IMG_FRAME);
    source->AddResourcePath("img/line.svg",     IDR_WALLET_IMG_LINE);
    source->AddResourcePath("img/settings-arrow.png",  IDR_WALLET_IMG_SETTINGS_ARROW);
    source->AddResourcePath("img/success.png",  IDR_WALLET_IMG_SUCCESS);


    source->AddResourcePath("fonts/Montserrat/Montserrat-Black.ttf",        IDR_WALLET_FONT_BLACK);
    source->AddResourcePath("fonts/Montserrat/Montserrat-Bold.ttf",         IDR_WALLET_FONT_BOLD);
    source->AddResourcePath("fonts/Montserrat/Montserrat-ExtraLight.ttf",   IDR_WALLET_FONT_EXTRALIGHT);
    source->AddResourcePath("fonts/Montserrat/Montserrat-Light.ttf",        IDR_WALLET_FONT_LIGHT);
    source->AddResourcePath("fonts/Montserrat/Montserrat-Medium.ttf",       IDR_WALLET_FONT_MEDIUM);
    source->AddResourcePath("fonts/Montserrat/Montserrat-Regular.ttf",      IDR_WALLET_FONT_REGULAR);
    source->AddResourcePath("fonts/Montserrat/Montserrat-SemiBold.ttf",     IDR_WALLET_FONT_SEMIBOLD);

    source->DisableTrustedTypesCSP();

    source->SetDefaultResource(IDR_WALLET_HTML);

    return source;
}


MobileWalletUI::MobileWalletUI(content::WebUI* web_ui) : content::WebUIController(web_ui)
{
    Profile* profile = Profile::FromWebUI(web_ui);

    web_ui->AddMessageHandler(std::make_unique<WalletMobileDOMHandler>());

    content::WebUIDataSource::Add(profile, CreateVersionUIDataSource());
}

MobileWalletUI::~MobileWalletUI()
{
}

base::RefCountedMemory* MobileWalletUI::GetFaviconResourceBytes(ui::ScaleFactor scale_factor)
{
    int favicon_size = static_cast<int>(gfx::kFaviconSize * ui::GetScaleForScaleFactor(scale_factor) + 0.5f);

    gfx::Image img(gfx::CreateVectorIcon(kProductIcon, favicon_size, SK_ColorWHITE));

    return img.As1xPNGBytes().release();

    /* PNG version
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_FAVICON_WALLET, scale_factor);
    */
}