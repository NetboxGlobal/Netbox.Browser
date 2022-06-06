#include "chrome/browser/ui/views/toolbar/netbox_wallet_button.h"

#include <sstream>
#include <string>
#include <iomanip>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "chrome/browser/netbox/environment/controller/wallet_environment.h"
#include "chrome/browser/netbox/wallet_manager/wallet_manager.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/view_class_properties.h"

#include "chrome/app/vector_icons/vector_icons.h"

class HighlightPathGenerator : public views::HighlightPathGenerator
{
public:
    SkPath GetHighlightPath(const views::View* view) override
    {
        gfx::Rect rect(view->size());
        rect.Inset(GetToolbarInkDropInsets(view));
        rect.set_x(rect.x() - GetLayoutConstant(TOOLBAR_ELEMENT_PADDING) / 2);
        rect.set_width(rect.width() + GetLayoutConstant(TOOLBAR_ELEMENT_PADDING));

        const int radii = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(views::Emphasis::kMaximum, rect.size());

        SkPath path;
        path.addRoundRect(gfx::RectToSkRect(rect), radii, radii);
        return path;
    }
};

NetboxWalletButton::NetboxWalletButton(PressedCallback callback, const std::u16string& initial_text)
    :  views::LabelButton(std::move(callback)) {

  views::HighlightPathGenerator::Install(this, std::make_unique<HighlightPathGenerator>());

  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));

  ConfigureInkDropForToolbar(this);  
  SetInstallFocusRingOnFocus(true);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetImageCentered(false);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  SetText(initial_text);

  base::PostTask(
      FROM_HERE,
      {content::BrowserThread::UI},
      base::BindOnce(&NetboxWalletButton::add_registrars, base::Unretained(this))
  );
}
    
NetboxWalletButton::~NetboxWalletButton()
{
}

const char* NetboxWalletButton::GetClassName() const {
  return "NetboxWalletButton";
}

/*std::unique_ptr<views::InkDrop> NetboxWalletButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(true);
  return std::move(ink_drop);
}*/

//std::unique_ptr<views::InkDropHighlight> NetboxWalletButton::CreateInkDropHighlight() const {
//  return CreateToolbarInkDropHighlight(this);
//} 

void NetboxWalletButton::add_registrars() {

  registrar_.Add(this,
                 chrome::NotificationType::NOTIFICATION_NETBOX_UPDATE_BALANCE,
                 content::NotificationService::AllSources());

  registrar_.Add(this,
                 chrome::NotificationType::NOTIFICATION_NETBOX_WALLET_STATUS_CHANGED,
                 content::NotificationService::AllSources());
}

void NetboxWalletButton::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {    
    
    if (chrome::NotificationType::NOTIFICATION_NETBOX_UPDATE_BALANCE == type)
    {
		if (Netboxglobal::WalletSessionManager::DS_OK == status_) 
		{
			SetTooltipText({});

			double* balance = content::Details<double>(details).ptr();

			if (nullptr != balance)
			{
				double val = *balance;
				val = floor(val * 100.0) / 100.0;

				std::stringstream strm;
				strm << std::setprecision(2) << std::fixed << val;

				SetText(base::ASCIIToUTF16(strm.str()));
			}
		}
    } 
    else if (chrome::NotificationType::NOTIFICATION_NETBOX_WALLET_STATUS_CHANGED == type) 
    {
        int32_t* status_ptr = content::Details<int32_t>(details).ptr();

        if (nullptr != status_ptr)
        {
            status_ = *status_ptr;
			
			VLOG(NETBOX_LOG_LEVEL) << "*** NetboxWalletButton::Observe status::" << status_;

            if (Netboxglobal::WalletSessionManager::DS_NONE == status_)
            {
                SetText(u"Loading");
                SetTooltipText(u"Starting Netbox.Wallet process");
            }
            else if (Netboxglobal::WalletSessionManager::DS_AUTH_COOKIE_ERROR == status_)
            {
				VLOG(NETBOX_LOG_LEVEL) << "*** NetboxWalletButton::Observe is_auth set false";
                SetText(u"Sign in ");
                SetTooltipText(u"Please sign in");
            }
            else if (status_ != Netboxglobal::WalletSessionManager::DS_OK)
            {
                SetText(u"Error");
                SetTooltipText(u"Netbox.Wallet is unavailable due to error");
            }
        }
    }
}

void NetboxWalletButton::OnThemeChanged() {
    views::LabelButton::OnThemeChanged();

    const ui::ThemeProvider* theme_provider = GetThemeProvider();
    if (!theme_provider)
        return;

    const SkColor color =
        theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);

    SetEnabledTextColors(color);  

    SchedulePaint();
}