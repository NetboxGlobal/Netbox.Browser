package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.provider.Settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.util.HashUtil;

public class NetboxHelper {
    private static final String SALT = "NetboxDeviceId";

    @SuppressLint("HardwareIds")
    @CalledByNative
    public static String GetDeviceId()
    {
        String android_id = Settings.Secure.getString(ContextUtils.getApplicationContext().getContentResolver(), Settings.Secure.ANDROID_ID);
        if (android_id == null)
        {
            return "";
        }

        String md5_hash = HashUtil.getMd5Hash(
                new HashUtil.Params(android_id).withSalt(SALT));

        return md5_hash == null ? "" : md5_hash;
    }

    @CalledByNative
    public static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic")
                || Build.FINGERPRINT.startsWith("unknown")
                || Build.MODEL.contains("google_sdk")
                || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                || (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic"))
                || "google_sdk".equals(Build.PRODUCT);
    }

}