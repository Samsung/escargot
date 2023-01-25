package com.samsung.lwe;

public class Escargot {
    static {
        System.loadLibrary("escargot");
        System.loadLibrary("escargot-android");
    }
    native static public void init();
    native static public void eval(String code);
}
