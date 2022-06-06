// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/netbox_activity_java_helper.h"

#include "chrome/android/chrome_jni_headers/NetboxActivityJavaHelper_jni.h"
#include "chrome/browser/netbox_activity/activity_watcher.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

ActivityNetboxHelper::ActivityNetboxHelper(JNIEnv* env, const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

ActivityNetboxHelper::~ActivityNetboxHelper() = default;

void ActivityNetboxHelper::SetFocus(JNIEnv* env, jboolean has_focus)
{
    if (has_focus)
    {
        ActivityWatcher::GetInstance().ui_on_focus(true);
    }
    else
    {
        ActivityWatcher::GetInstance().ui_on_focus(false);
    }
}

void ActivityNetboxHelper::OnDestroyed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_ActivityNetboxHelper_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new ActivityNetboxHelper(env, obj));
}
