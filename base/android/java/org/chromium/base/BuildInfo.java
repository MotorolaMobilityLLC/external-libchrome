// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.util.Log;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo}
 * information. This is primarly of use for accessesing package information
 * from native code.
 */
public class BuildInfo {
  private static final String TAG = "BuildInfo";
  private static final int MAX_FINGERPRINT_LENGTH = 128;

  /**
   * BuildInfo is a static utility class and therefore should'nt be
   * instantiated.
   */
  private BuildInfo() {
  }

  @CalledByNative
  public static String getDevice() {
    return Build.DEVICE;
  }

  @CalledByNative
  public static String getBrand() {
    return Build.BRAND;
  }

  @CalledByNative
  public static String getAndroidBuildId() {
    return Build.ID;
  }

  /**
   * @return The build fingerprint for the current Android install.  The value is truncated to a
   *         128 characters as this is used for crash and UMA reporting, which should avoid huge
   *         strings.
   */
  @CalledByNative
  public static String getAndroidBuildFingerprint() {
      return Build.FINGERPRINT.substring(
              0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));
  }

  @CalledByNative
  public static String getDeviceModel() {
    return Build.MODEL;
  }

  @CalledByNative
  public static String getPackageVersionCode(Context context) {
    String msg = "versionCode not available.";
    try {
      PackageManager pm = context.getPackageManager();
      PackageInfo pi = pm.getPackageInfo("com.android.chrome", 0);
      msg = "" + pi.versionCode;
    } catch (NameNotFoundException e) {
      Log.d(TAG, msg);
    }
    return msg;

  }

  @CalledByNative
  public static String getPackageVersionName(Context context) {
    String msg = "versionName not available";
    try {
      PackageManager pm = context.getPackageManager();
      PackageInfo pi = pm.getPackageInfo("com.android.chrome", 0);
      msg = pi.versionName;
    } catch (NameNotFoundException e) {
      Log.d(TAG, msg);
    }
    return msg;
  }

}
