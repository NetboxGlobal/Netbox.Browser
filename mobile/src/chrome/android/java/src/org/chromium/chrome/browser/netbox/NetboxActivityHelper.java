package org.chromium.chrome.browser.netbox;

import org.chromium.base.annotations.NativeMethods;

public final class NetboxActivityHelper  {

    private NetboxActivityHelper() { }

    public static void OnFocus(boolean focus)
    {
        NetboxActivityHelperJni.get().OnFocus(focus);
    }

    @NativeMethods
    interface Natives {
        void OnFocus(boolean focus);
    }
}
