// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;
import android.text.TextUtils;

import junit.framework.TestCase;
import junit.framework.TestResult;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.test.reporter.TestStatusListener;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

// TODO(jbudorick): Add support for on-device handling of timeouts.
/**
 *  An Instrumentation test runner that checks SDK level for tests with specific requirements.
 */
public class BaseInstrumentationTestRunner extends InstrumentationTestRunner {
    private static final String TAG = "cr.base.test";

    private static final int SLEEP_INTERVAL = 50; // milliseconds
    private static final int WAIT_DURATION = 5000; // milliseconds

    /**
     * An interface for classes that check whether a test case should be skipped.
     */
    public interface SkipCheck {
        /**
         * Checks whether the given test case should be skipped.
         *
         * @param testCase The test case to check.
         * @return Whether the test case should be skipped.
         */
        public boolean shouldSkip(TestCase testCase);
    }

    /**
     * A test result that can skip tests.
     */
    public class BaseTestResult extends TestResult {

        private final List<SkipCheck> mSkipChecks;

        /**
         * Creates an instance of BaseTestResult.
         */
        public BaseTestResult() {
            mSkipChecks = new ArrayList<SkipCheck>();
        }

        /**
         * Adds a check for whether a test should run.
         *
         * @param skipCheck The check to add.
         */
        public void addSkipCheck(SkipCheck skipCheck) {
            mSkipChecks.add(skipCheck);
        }

        private boolean shouldSkip(final TestCase test) {
            for (SkipCheck s : mSkipChecks) {
                if (s.shouldSkip(test)) return true;
            }
            return false;
        }

        @Override
        protected void run(final TestCase test) {
            if (shouldSkip(test)) {
                startTest(test);

                Bundle skipResult = new Bundle();
                skipResult.putString("class", test.getClass().getName());
                skipResult.putString("test", test.getName());
                skipResult.putBoolean("test_skipped", true);
                sendStatus(0, skipResult);

                endTest(test);
            } else {
                try {
                    CommandLineFlags.setUp(
                            BaseInstrumentationTestRunner.this.getTargetContext(),
                            test.getClass().getMethod(test.getName()));
                } catch (NoSuchMethodException e) {
                    Log.e(TAG, "Unable to set up CommandLineFlags", e);
                }
                super.run(test);
            }
        }
    }

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        AndroidTestRunner runner = new AndroidTestRunner() {
            @Override
            protected TestResult createTestResult() {
                BaseTestResult r = new BaseTestResult();
                addSkipChecks(r);
                return r;
            }
        };
        runner.addTestListener(new TestStatusListener(getContext()));
        return runner;
    }

    /**
     * Adds the desired SkipChecks to result. Subclasses can add additional SkipChecks.
     */
    protected void addSkipChecks(BaseTestResult result) {
        result.addSkipCheck(new MinAndroidSdkLevelSkipCheck());
        result.addSkipCheck(new RestrictionSkipCheck());
    }

    /**
     * Checks if any restrictions exist and skip the test if it meets those restrictions.
     */
    public class RestrictionSkipCheck implements SkipCheck {
        public boolean shouldSkip(TestCase testCase) {
            Method method;
            try {
                method = testCase.getClass().getMethod(testCase.getName(), (Class[]) null);
            } catch (NoSuchMethodException e) {
                Log.e(TAG, "Unable to find %s in %s", testCase.getName(),
                        testCase.getClass().getName(), e);
                return true;
            }
            Restriction restrictions = method.getAnnotation(Restriction.class);
            if (restrictions != null) {
                for (String restriction : restrictions.value()) {
                    if (restrictionApplies(restriction)) {
                        return true;
                    }
                }
            }
            return false;
        }

        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_LOW_END_DEVICE)
                    && !SysUtils.isLowEndDevice()) {
                return true;
            }
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
                    && SysUtils.isLowEndDevice()) {
                return true;
            }
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_INTERNET)
                    && !isNetworkAvailable()) {
                return true;
            }
            return false;
        }

        private boolean isNetworkAvailable() {
            final ConnectivityManager connectivityManager = (ConnectivityManager)
                    getTargetContext().getSystemService(Context.CONNECTIVITY_SERVICE);
            final NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
            return activeNetworkInfo != null && activeNetworkInfo.isConnected();
        }
    }

    /**
     * Checks the device's SDK level against any specified minimum requirement.
     */
    public static class MinAndroidSdkLevelSkipCheck implements SkipCheck {

        /**
         * If {@link org.chromium.base.test.util.MinAndroidSdkLevel} is present, checks its value
         * against the device's SDK level.
         *
         * @param testCase The test to check.
         * @return true if the device's SDK level is below the specified minimum.
         */
        @Override
        public boolean shouldSkip(TestCase testCase) {
            Class<?> testClass = testCase.getClass();
            if (testClass.isAnnotationPresent(MinAndroidSdkLevel.class)) {
                MinAndroidSdkLevel v = testClass.getAnnotation(MinAndroidSdkLevel.class);
                if (Build.VERSION.SDK_INT < v.value()) {
                    Log.i(TAG, "Test " + testClass.getName() + "#" + testCase.getName()
                            + " is not enabled at SDK level " + Build.VERSION.SDK_INT
                            + ".");
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * Gets the target context.
     *
     * On older versions of Android, getTargetContext() may initially return null, so we have to
     * wait for it to become available.
     *
     * @return The target {@link android.content.Context} if available; null otherwise.
     */
    @Override
    public Context getTargetContext() {
        Context targetContext = super.getTargetContext();
        try {
            long startTime = SystemClock.uptimeMillis();
            // TODO(jbudorick): Convert this to CriteriaHelper once that moves to base/.
            while (targetContext == null
                    && SystemClock.uptimeMillis() - startTime < WAIT_DURATION) {
                Thread.sleep(SLEEP_INTERVAL);
                targetContext = super.getTargetContext();
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted while attempting to initialize the command line.");
        }
        return targetContext;
    }
}
