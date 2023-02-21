package com.samsung.lwe.escargot;

public class Escargot {
    static {
        System.loadLibrary("escargot");
        System.loadLibrary("escargot-android");
    }

    static native public void init();
}
