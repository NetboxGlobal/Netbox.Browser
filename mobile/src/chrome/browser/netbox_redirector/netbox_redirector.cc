#include "chrome/browser/netbox_redirector/netbox_redirector.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"


NetboxRedirectorTabHelper::NetboxRedirectorTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

NetboxRedirectorTabHelper::~NetboxRedirectorTabHelper() = default;


void NetboxRedirectorTabHelper::DidFinishNavigation(content::NavigationHandle* navigation_handle)
{
    if (navigation_handle && navigation_handle->IsInMainFrame())
    {
        GURL url = web_contents()->GetLastCommittedURL();

        if ("account.netbox.global" == url.host() || "devaccount.netbox.global" == url.host())
        {
            if ("/wallet" == url.path())
            {
                return netbox_redirect(GURL("chrome://wallet"));
            }

            if ("/buy" == url.path())
            {
                return netbox_redirect(GURL("chrome://wallet/?buy"));
            }
        }
    }
}


void NetboxRedirectorTabHelper::netbox_redirect(const GURL url)
{
    content::NavigationController& controller = web_contents()->GetController();

    controller.LoadURL(
        url,
        content::Referrer(),
        ui::PageTransition::PAGE_TRANSITION_TYPED,
        std::string());
}


WEB_CONTENTS_USER_DATA_KEY_IMPL(NetboxRedirectorTabHelper)
