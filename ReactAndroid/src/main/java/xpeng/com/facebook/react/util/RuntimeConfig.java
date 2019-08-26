package xpeng.com.facebook.react.util;

import com.facebook.react.BuildConfig;

public class RuntimeConfig {
    private static final boolean sSplitRamBundle = BuildConfig.XPENG_BUILD_SPLIT_BUNDLE;

    public static boolean isSplitRamBundle() {
        return sSplitRamBundle;
    }
}
