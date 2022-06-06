#include "chrome/android/chrome_jni_headers/NetboxActivityHelper_jni.h"
#include "chrome/browser/netbox_activity/activity_watcher.h"

using base::android::JavaParamRef;

static void JNI_NetboxActivityHelper_OnFocus(JNIEnv* env, jboolean focus) {
    Netboxglobal::ActivityWatcher::GetInstance()->ui_on_focus(focus);
}


