#ifndef CHROME_BROWSER_NETBOX_REDIRECTOR_NETBOX_REDIRECTOR_H_
#define CHROME_BROWSER_NETBOX_REDIRECTOR_NETBOX_REDIRECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"

class NetboxRedirectorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NetboxRedirectorTabHelper> {
public:
    ~NetboxRedirectorTabHelper() override;

    void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;

private:
    explicit NetboxRedirectorTabHelper(content::WebContents* web_contents);
    friend class content::WebContentsUserData<NetboxRedirectorTabHelper>;

    void netbox_redirect(const GURL url);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NETBOX_REDIRECTOR_NETBOX_REDIRECTOR_H_
