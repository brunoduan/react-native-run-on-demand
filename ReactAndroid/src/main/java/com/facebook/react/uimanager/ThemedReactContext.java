/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.react.uimanager;

import javax.annotation.Nullable;

import android.app.Activity;
import android.content.Context;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.LifecycleEventListener;

/* XPENG_BUILD_TRACKER */
import xpeng.com.facebook.react.tracker.TrackerListener;
/* XPENG_BUILD_TRACKER */

/**
 * Wraps {@link ReactContext} with the base {@link Context} passed into the constructor.
 * It provides also a way to start activities using the viewContext to which RN native views belong.
 * It delegates lifecycle listener registration to the original instance of {@link ReactContext}
 * which is supposed to receive the lifecycle events. At the same time we disallow receiving
 * lifecycle events for this wrapper instances.
 * TODO: T7538544 Rename ThemedReactContext to be in alignment with name of ReactApplicationContext
 */
public class ThemedReactContext extends ReactContext {

  private final ReactApplicationContext mReactApplicationContext;

  public ThemedReactContext(ReactApplicationContext reactApplicationContext, Context base) {
    super(base);
    initializeWithInstance(reactApplicationContext.getCatalystInstance());
    mReactApplicationContext = reactApplicationContext;
  }

  @Override
  public void addLifecycleEventListener(LifecycleEventListener listener) {
    mReactApplicationContext.addLifecycleEventListener(listener);
  }

  @Override
  public void removeLifecycleEventListener(LifecycleEventListener listener) {
    mReactApplicationContext.removeLifecycleEventListener(listener);
  }

  @Override
  public boolean hasCurrentActivity() {
    return mReactApplicationContext.hasCurrentActivity();
  }

  @Override
  public @Nullable Activity getCurrentActivity() {
    return mReactApplicationContext.getCurrentActivity();
  }

  /* XPENG_BUILD_TRACKER */
  public @Nullable
  TrackerListener getTrackerListener() {
    return mReactApplicationContext.getTrackerListener();
  }
  /* XPENG_BUILD_TRACKER */
}
