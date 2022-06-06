package org.chromium.chrome.browser.netbox;

import android.content.Context;
import android.content.Intent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.ContextUtils;


import org.chromium.base.Log;

public class NetboxShare {
    private static final String TAG = "chromium";

    @CalledByNative
    public static boolean Share(String link, String share_text)
    {
        Intent shareIntent = new Intent(android.content.Intent.ACTION_SEND);
        if (null == shareIntent)
        {
            return false;
        }

        shareIntent.setType("text/plain");
        shareIntent.putExtra(Intent.EXTRA_SUBJECT, "Netbox.Browser");
        shareIntent.putExtra(Intent.EXTRA_TEXT, share_text + ' ' + link);

        Intent chooserIntent = Intent.createChooser(shareIntent, null);
        if (null == chooserIntent)
        {
            return false;
        }
        chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ContextUtils.getApplicationContext().startActivity(chooserIntent);

        return true;
    }
}