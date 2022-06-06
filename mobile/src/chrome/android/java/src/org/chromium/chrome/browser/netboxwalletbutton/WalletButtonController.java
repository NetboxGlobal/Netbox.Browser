// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.netboxwalletbutton;

import org.chromium.base.Log;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View.OnClickListener;
import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.content_public.browser.LoadUrlParams;

public class WalletButtonController implements ButtonDataProvider{
    private static final String TAG = "chromium";

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    private ButtonData mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private OnClickListener mOnClickListener;

    /**
     * Creates ShareButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     */
    public WalletButtonController(
            Context context,
            ActivityTabProvider tabProvider
            ) {
        mContext = context;

        mTabProvider = tabProvider;

        mOnClickListener = ((view) -> {
            Activity activity_raw = ApplicationStatus.getLastTrackedFocusedActivity();
            if (activity_raw != null && activity_raw instanceof ChromeActivity)
            {
                ChromeActivity activity = (ChromeActivity) activity_raw;

                TabModelSelector selector = activity.getTabModelSelector();

                if (null != selector)
                {
                    if (findAndActivateWalletTab(selector.getModel(false)) || findAndActivateWalletTab(selector.getModel(true)))
                    {
                        return;
                    }
                }
            }


            if (null == mTabProvider)
            {
                return;
            }

            Tab tab = mTabProvider.get();
            if (tab == null)
            {
                return;
            }

            //LoadUrlParams loadUrlParams = new LoadUrlParams("chrome://wallet");
            LoadUrlParams loadUrlParams = new LoadUrlParams("https://twitter.com/netboxglobal");
            tab.loadUrl(loadUrlParams);
        });

        mButtonData = new ButtonData(false,
                AppCompatResources.getDrawable(mContext, R.drawable.ic_toolbar_walletbutton_24dp),
                mOnClickListener, R.string.share, true, null, true);
    }

    @Override
    public void destroy() {
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        updateButtonVisibility(tab);
        return mButtonData;
    }

    private void updateButtonVisibility(Tab tab) {
        mButtonData.canShow = true;
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    private boolean findAndActivateWalletTab(TabModel tabModel)
    {
        if (null == tabModel)
        {
            return false;
        }

        for (int i = 0; i < tabModel.getCount(); i++)
        {
            Tab tab = tabModel.getTabAt(i);
            if (tab == null)
            {
                continue;
            }

            Uri current = Uri.parse(tab.getUrlString());
            if (current == null)
            {
                continue;
            }

            if ((current.getScheme().equals("chrome")) && current.getHost().equals("wallet"))
            {
                if (tab.isHidden())
                {
                    tab.show(TabSelectionType.FROM_USER);
                }
                else
                {
                    tab.reload();
                }

                int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
                TabModelUtils.setIndex(tabModel, tabIndex);

                return true;
            }
        }

        return false;
    }
}
