// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher.AccessPoint;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * A controller for configuring the sign in promo. It sets up the sign in promo depending on the
 * context: whether there are any Google accounts on the device which have been previously signed in
 * or not. The controller also takes care of counting impressions, recording signin related user
 * actions and histograms.
 */
public class SigninPromoController {
    /**
     * Receives notifications when user clicks close button in the promo.
     */
    public interface OnDismissListener {
        /**
         * Action to be performed when the promo is being dismissed.
         */
        void onDismiss();
    }

    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int MAX_IMPRESSIONS_SETTINGS = 20;

    private @Nullable DisplayableProfileData mProfileData;
    private @Nullable ImpressionTracker mImpressionTracker;
    private final OneShotImpressionListener mImpressionFilter =
            new OneShotImpressionListener(this::recordSigninPromoImpression);
    private final @AccessPoint int mAccessPoint;
    private final @Nullable String mImpressionCountName;
    private final String mImpressionUserActionName;
    private final String mImpressionWithAccountUserActionName;
    private final String mImpressionWithNoAccountUserActionName;
    private final String mSigninWithDefaultUserActionName;
    private final String mSigninNotDefaultUserActionName;
    private final String mSigninNewAccountUserActionName;
    private final @Nullable String mImpressionsTilDismissHistogramName;
    private final @Nullable String mImpressionsTilSigninButtonsHistogramName;
    private final @Nullable String mImpressionsTilXButtonHistogramName;
    private final @StringRes int mDescriptionStringId;
    private final @StringRes int mDescriptionStringIdNoAccount;
    private final SigninActivityLauncher mSigninActivityLauncher;
    private boolean mWasDisplayed;
    private boolean mWasUsed;

    /**
     * Determines whether the impression limit has been reached for the given access point.
     * @param accessPoint The access point for which the impression limit is being checked.
     */
    public static boolean hasNotReachedImpressionLimit(@AccessPoint int accessPoint) {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return getSigninPromoImpressionsCountBookmarks() < MAX_IMPRESSIONS_BOOKMARKS;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                return false; /* netboxcomment, always false
                int maxImpressions = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                        "MaxSigninPromoImpressions", Integer.MAX_VALUE);
                return SharedPreferencesManager.getInstance().readInt(
                               ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP)
                        < maxImpressions;
                netboxcomment end */
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                return true;
            case SigninAccessPoint.SETTINGS:
                return preferencesManager.readInt(
                               ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS)
                        < MAX_IMPRESSIONS_SETTINGS;
            default:
                assert false : "Unexpected value for access point: " + accessPoint;
                return false;
        }
    }

    /**
     * Creates a new SigninPromoController.
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     * @param signinActivityLauncher Launcher of {@link SigninActivity}.
     */
    public SigninPromoController(
            @AccessPoint int accessPoint, SigninActivityLauncher signinActivityLauncher) {
        mAccessPoint = accessPoint;
        mSigninActivityLauncher = signinActivityLauncher;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionCountName =
                        ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS;
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromBookmarkManager";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromBookmarkManager";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromBookmarkManager";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromBookmarkManager";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromBookmarkManager";
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton";
                mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_bookmarks_no_account;
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP;
                mImpressionUserActionName = "Signin_Impression_FromNTPContentSuggestions";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromNTPContentSuggestions";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromNTPContentSuggestions";
                mSigninWithDefaultUserActionName =
                        "Signin_SigninWithDefault_FromNTPContentSuggestions";
                mSigninNotDefaultUserActionName =
                        "Signin_SigninNotDefault_FromNTPContentSuggestions";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromNTPContentSuggestions";
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
                mDescriptionStringId = R.string.signin_promo_description_ntp_content_suggestions;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_ntp_content_suggestions_no_account;
                break;
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                mImpressionCountName = null;
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromRecentTabs";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromRecentTabs";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromRecentTabs";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromRecentTabs";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromRecentTabs";
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
                mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_recent_tabs_no_account;
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS;
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mImpressionWithAccountUserActionName = "Signin_ImpressionWithAccount_FromSettings";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromSettings";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromSettings";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromSettings";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromSettings";
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilXButton";
                mDescriptionStringId = R.string.signin_promo_description_settings;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_settings_no_account;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /**
     * Called when the signin promo is destroyed.
     */
    public void onPromoDestroyed() {
        if (!mWasDisplayed || mWasUsed || mImpressionsTilDismissHistogramName == null) {
            return;
        }
        RecordHistogram.recordCount100Histogram(
                mImpressionsTilDismissHistogramName, getNumImpressions());
    }

    /**
     * Configures the signin promo view and resets the impression tracker. If this controller has
     * been previously set up, it should be {@link #detach detached} first.
     * @param view The view in which the promo will be added.
     * @param profileData If not null, the promo will be configured to be in the hot state, using
     *         the account image, email and full name of the user to set the picture and the text of
     *         the promo appropriately. Otherwise, the promo will be in the cold state.
     * @param onDismissListener Listener which handles the action of dismissing the promo. A null
     *         onDismissListener marks that the promo is not dismissible and as a result the close
     *         button is hidden.
     */
    public void setupPromoView(Context context, PersonalizedSigninPromoView view,
            final @Nullable DisplayableProfileData profileData,
            final @Nullable OnDismissListener onDismissListener) {
        mProfileData = profileData;
        mWasDisplayed = true;

        assert mImpressionTracker
                == null : "detach() should be called before setting up a new view";
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(mImpressionFilter);

        if (mProfileData == null) {
            setupColdState(context, view);
        } else {
            setupHotState(context, view);
        }

        if (onDismissListener != null) {
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton().setOnClickListener(promoView -> {
                assert mImpressionsTilXButtonHistogramName != null;
                mWasUsed = true;
                RecordHistogram.recordCount100Histogram(
                        mImpressionsTilXButtonHistogramName, getNumImpressions());
                onDismissListener.onDismiss();
            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /**
     * Should be called when the view is not in use anymore (e.g. it's being recycled).
     */
    public void detach() {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    /** @return the resource used for the text displayed as promo description. */
    public @StringRes int getDescriptionStringId() {
        return mProfileData == null ? mDescriptionStringIdNoAccount : mDescriptionStringId;
    }

    private void setupColdState(final Context context, PersonalizedSigninPromoView view) {
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        view.getDescription().setText(mDescriptionStringIdNoAccount);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        } else {
            view.getPrimaryButton().setText(R.string.sign_in_to_chrome);
        }
        view.getPrimaryButton().setOnClickListener(v -> signinWithNewAccount(context));

        view.getSecondaryButton().setVisibility(View.GONE);
    }

    private void setupHotState(final Context context, PersonalizedSigninPromoView view) {
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.signin_promo_account_image_size);

        view.getDescription().setText(mDescriptionStringId);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        } else {
            String signinButtonText = context.getString(R.string.signin_promo_continue_as,
                    mProfileData.getGivenNameOrFullNameOrEmail());
            view.getPrimaryButton().setText(signinButtonText);
        }
        view.getPrimaryButton().setOnClickListener(v -> signinWithDefaultAccount(context));

        view.getSecondaryButton().setText(R.string.signin_promo_choose_another_account);
        view.getSecondaryButton().setOnClickListener(v -> signinWithNotDefaultAccount(context));
        view.getSecondaryButton().setVisibility(View.VISIBLE);
    }

    private int getNumImpressions() {
        return SharedPreferencesManager.getInstance().readInt(mImpressionCountName);
    }

    private void signinWithNewAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNewAccountUserActionName);
        mSigninActivityLauncher.launchActivityForPromoAddAccountFlow(context, mAccessPoint);
    }

    private void signinWithDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninWithDefaultUserActionName);
        mSigninActivityLauncher.launchActivityForPromoDefaultFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void signinWithNotDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNotDefaultUserActionName);
        mSigninActivityLauncher.launchActivityForPromoChooseAccountFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void recordSigninButtonUsed() {
        mWasUsed = true;
        if (mImpressionsTilSigninButtonsHistogramName != null) {
            RecordHistogram.recordCount100Histogram(
                    mImpressionsTilSigninButtonsHistogramName, getNumImpressions());
        }
    }

    private void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private void recordSigninPromoImpression() {
        RecordUserAction.record(mImpressionUserActionName);
        if (mProfileData == null) {
            RecordUserAction.record(mImpressionWithNoAccountUserActionName);
        } else {
            RecordUserAction.record(mImpressionWithAccountUserActionName);
        }

        // If mImpressionCountName is not null then we should record impressions.
        if (mImpressionCountName != null) {
            SharedPreferencesManager.getInstance().incrementInt(mImpressionCountName);
        }
    }

    @VisibleForTesting
    public static void setSigninPromoImpressionsCountBookmarksForTests(int count) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS, count);
    }

    @VisibleForTesting
    public static int getSigninPromoImpressionsCountBookmarks() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS);
    }

    @VisibleForTesting
    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }
}
