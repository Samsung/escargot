package com.samsung.lwe.escargot;

public class Escargot {
    static {
        System.loadLibrary("escargot-jni");
    }

    static native public void init();
}
