#ifndef NETBOXGLOBAL_ACTIVITY_ACTIVITY_JAVA_HELPER_H_
#define NETBOXGLOBAL_ACTIVITY_ACTIVITY_JAVA_HELPER_H_

#include "base/android/scoped_java_ref.h"

// Native Favicon provider for Tab. Managed by Java layer.
class ActivityNetboxHelper {
public:
    ActivityNetboxHelper(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
    ~ActivityNetboxHelper();

    void SetFocus(JNIEnv* env, jboolean has_focus);

    void OnDestroyed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

private:
    base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

#endif  // NETBOXGLOBAL_ACTIVITY_ACTIVITY_JAVA_HELPER_H_
