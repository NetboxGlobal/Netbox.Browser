package org.chromium.chrome.browser.netbox;

import android.content.Context;
import android.net.Uri;
import android.os.RemoteException;
import com.android.installreferrer.api.InstallReferrerClient;
import com.android.installreferrer.api.InstallReferrerClient.InstallReferrerResponse;
import com.android.installreferrer.api.InstallReferrerStateListener;
import com.android.installreferrer.api.ReferrerDetails;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.NetboxWalletHelper;
import org.chromium.chrome.browser.netbox.NetboxActivityHelper;


public class NetboxReferrer implements InstallReferrerStateListener {
    private static final String TAG = "chromium";

    private static NetboxReferrer sInstance;
    private InstallReferrerClient referrerClient_;

    private NetboxReferrer() {}

    public static NetboxReferrer getInstance() {
        if (sInstance != null) return sInstance;
        sInstance = new NetboxReferrer();
        return sInstance;
    }

    public void initReferrer(Context context) {
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK,
            new InitReferrerRunnable(context, this));
    }

    private class InitReferrerRunnable implements Runnable
    {
        private Context context_;
        private NetboxReferrer referrer_;

        public InitReferrerRunnable(Context context, NetboxReferrer netboxReferrer)
        {
            context_ = context;
            referrer_ = netboxReferrer;
        }

        @Override
        public void run()
        {
            if (NetboxWalletHelper.HasReferrer())
            {
                return;
            }

            referrerClient_ = InstallReferrerClient.newBuilder(context_).build();

            try {
                referrerClient_.startConnection(referrer_);

            } catch (SecurityException e) {
                Log.e(TAG, "# " + e);
            }
        }
    }

    private String FilterReferrer(String referrer)
    {
        if (referrer != null)
        {
            return referrer.replaceAll("[^a-zA-Z0-9]", "");
        }

        return "";
    }

    @Override
    public void onInstallReferrerSetupFinished(int responseCode)
    {
        if (InstallReferrerResponse.OK != responseCode)
        {
            return;
        }

        try{
            ReferrerDetails response = referrerClient_.getInstallReferrer();

            if (null != response)
            {
                Uri uri = Uri.parse("http://netbox.global/?" + response.getInstallReferrer());
                Log.e(TAG, "referrer, playmarket %s", response.getInstallReferrer());

                String referrer = FilterReferrer(uri.getQueryParameter("netbox"));

                NetboxWalletHelper.SetReferrer(referrer);
            }

            referrerClient_.endConnection();
        }
        catch(RemoteException e)
        {
        }
    }

    @Override
    public void onInstallReferrerServiceDisconnected()
    {
    }
}
