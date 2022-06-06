package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.SharedPreferences;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.ContextUtils;

public class NetboxWalletHelper {

    private static final String WALLET_PREFS_NAME       = "NetboxWalletPrefs";
    private static final String WALLET_PREFS_SEED_KEY   = "seed";
    private static final String WALLET_PREFS_WALLET_KEY = "wallet58";
    private static final String WALLET_PREFS_XPUB_KEY   = "xpub";
    private static final String WALLET_PREFS_FIRST_ADDRESS_KEY = "first_address";
    private static final String WALLET_PREFS_ADDRESS_INDEX_KEY = "address_index";
    private static final String WALLET_PREFS_IS_TESTNET_KEY    = "testnet";
    private static final String REFERRER_KEY = "referrer";
    private static final String WALLET_PREFS_FEATURE    = "feature";

    private static final String TOKEN_FLAGS_KEY = "token_flags";
    private static final String TOKEN_LANG_KEY = "token_lang";
    private static final String TOKEN_FIRST_ADDRESS_KEY = "token_first_address";

    private static String get_key(String key_name, boolean is_qa)
    {
        if (is_qa)
        {
            return key_name + "testnet";
        }

        return key_name;
    }

    @CalledByNative
    public static boolean SetWallet(String seed, String wallet58, String xpub, String first_address, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

       sharedPref.edit()
        .putString(get_key(WALLET_PREFS_SEED_KEY, is_qa),           seed)
        .putString(get_key(WALLET_PREFS_WALLET_KEY, is_qa),         wallet58)
        .putString(get_key(WALLET_PREFS_XPUB_KEY, is_qa),           xpub)
        .putString(get_key(WALLET_PREFS_FIRST_ADDRESS_KEY, is_qa),  first_address)
        .remove(get_key(WALLET_PREFS_ADDRESS_INDEX_KEY, is_qa))
        .remove(get_key(TOKEN_FLAGS_KEY, is_qa))
        .remove(get_key(TOKEN_LANG_KEY, is_qa))
        .remove(get_key(TOKEN_FIRST_ADDRESS_KEY, is_qa))
        .apply();

        return true;
    }

    @CalledByNative
    public static boolean ResetWallet()
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        sharedPref.edit()
            .remove(get_key(WALLET_PREFS_SEED_KEY, true)).remove(get_key(WALLET_PREFS_SEED_KEY, false))
            .remove(get_key(WALLET_PREFS_WALLET_KEY, true)).remove(get_key(WALLET_PREFS_WALLET_KEY, false))
            .remove(get_key(WALLET_PREFS_XPUB_KEY, true)).remove(get_key(WALLET_PREFS_XPUB_KEY, false))
            .remove(get_key(WALLET_PREFS_FIRST_ADDRESS_KEY, true)).remove(get_key(WALLET_PREFS_FIRST_ADDRESS_KEY, false))
            .remove(get_key(WALLET_PREFS_ADDRESS_INDEX_KEY, true)).remove(get_key(WALLET_PREFS_ADDRESS_INDEX_KEY, false))
            .remove(get_key(TOKEN_FLAGS_KEY, true)).remove(get_key(TOKEN_FLAGS_KEY, false))
            .remove(get_key(TOKEN_LANG_KEY, true)).remove(get_key(TOKEN_LANG_KEY, false))
            .remove(get_key(TOKEN_FIRST_ADDRESS_KEY, true)).remove(get_key(TOKEN_FIRST_ADDRESS_KEY, false))
            .apply();

        return true;
    }

    public static boolean SetInternalString(String string_name, String string_value, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        sharedPref.edit()
            .putString(get_key(string_name, is_qa),           string_value)
            .apply();

        return true;
    }

    public static String GetInternalString(String key, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);
        try{
            return sharedPref.getString(get_key(key, is_qa), "");
        }
        catch (ClassCastException e) {
        }

        return "";
    }

    private static int GetInternalNumber(String key, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        try {
            return sharedPref.getInt(get_key(key, is_qa), 0);
        } catch (ClassCastException e) {
        }

        return 0;
    }

    @CalledByNative
    public static String GetSeed(boolean is_qa)
    {
        return GetInternalString(WALLET_PREFS_SEED_KEY, is_qa);
    }

    @CalledByNative
    public static String GetWallet(boolean is_qa)
    {
        return GetInternalString(WALLET_PREFS_WALLET_KEY, is_qa);
    }

    @CalledByNative
    public static String GetXPub(boolean is_qa)
    {
        return GetInternalString(WALLET_PREFS_XPUB_KEY, is_qa);
    }

    @CalledByNative
    public static String GetFirstAddress(boolean is_qa)
    {
        return GetInternalString(WALLET_PREFS_FIRST_ADDRESS_KEY, is_qa);
    }

    public static boolean HasReferrer()
    {
        if ("1".equals(NetboxWalletHelper.GetInternalString("has_referrer", false)))
        {
            return true;
        }

        return false;
    }

    @CalledByNative
    public static String GetReferrer()
    {
        return GetInternalString(REFERRER_KEY, false);
    }

    public static boolean SetReferrer(String referrer)
    {
        NetboxWalletHelper.SetInternalString("has_referrer", "1", false);
        NetboxWalletHelper.SetInternalString(REFERRER_KEY, referrer, false);

        return true;
    }

    @CalledByNative
    public static boolean SetAddressIndex(int address_index, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        sharedPref.edit()
            .putInt(get_key(WALLET_PREFS_ADDRESS_INDEX_KEY, is_qa),           address_index)
            .apply();

        return true;
    }

    @CalledByNative
    public static int GetAddressIndex(boolean is_qa)
    {
        int value = GetInternalNumber(WALLET_PREFS_ADDRESS_INDEX_KEY, is_qa);

        if (value <= 0)
        {
            value = 1;
        }

        return value;
    }

    @CalledByNative
    public static boolean SetIsTestnet(boolean is_testnet)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        if (is_testnet)
        {
            sharedPref.edit().putInt(get_key(WALLET_PREFS_IS_TESTNET_KEY, false),  1).apply();
        }
        else
        {
            sharedPref.edit().putInt(get_key(WALLET_PREFS_IS_TESTNET_KEY, false),  0).apply();
        }

        return true;
    }

    @CalledByNative
    public static boolean GetIsTestnet()
    {
        int value = GetInternalNumber(WALLET_PREFS_IS_TESTNET_KEY, false);

        if (value != 0)
        {
            return true;
        }

        return false;
    }

    @CalledByNative
    public static boolean SetFeature(String feature_name, boolean feature_enabled)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

        if (feature_enabled){
            sharedPref.edit()
                .putInt(get_key(WALLET_PREFS_FEATURE + feature_name, false), 1)
                .apply();
        }
        else {
            sharedPref.edit()
                .remove(get_key(WALLET_PREFS_FEATURE + feature_name, false))
                .apply();
        }

        return true;
    }

    @CalledByNative
    public static boolean GetFeature(String feature_name)
    {
        int value = GetInternalNumber(WALLET_PREFS_FEATURE + feature_name, false);

        if (value > 0)
        {
            return true;
        }

        return false;
    }

    @CalledByNative
    public static boolean SetTokenDetails(int flags, String lang, String firstaddress, boolean is_qa)
    {
        SharedPreferences sharedPref = ContextUtils.getApplicationContext().getSharedPreferences(
                WALLET_PREFS_NAME, Context.MODE_PRIVATE);

       sharedPref.edit()
        .putInt(get_key(TOKEN_FLAGS_KEY, is_qa),           flags)
        .putString(get_key(TOKEN_LANG_KEY, is_qa),         lang)
        .putString(get_key(TOKEN_FIRST_ADDRESS_KEY, is_qa),         firstaddress)
        .apply();

        return true;
    }

    @CalledByNative
    public static int GetTokenFlags(boolean is_qa)
    {
        return GetInternalNumber(TOKEN_FLAGS_KEY, is_qa);
    }

    @CalledByNative
    public static String GetTokenLang(boolean is_qa)
    {
        return GetInternalString(TOKEN_LANG_KEY, is_qa);
    }

    @CalledByNative
    public static String GetTokenFirstAddress(boolean is_qa)
    {
        return GetInternalString(TOKEN_FIRST_ADDRESS_KEY, is_qa);
    }
}