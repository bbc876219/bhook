package com.bbc;

public class NativeHookeeA {
    public static void test() {
        nativeTest();
    }

    private static native void nativeTest();
}
