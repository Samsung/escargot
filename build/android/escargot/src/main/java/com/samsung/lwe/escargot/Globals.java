package com.samsung.lwe.escargot;

public class Globals {
    static { Escargot.init(); }
    static public native void initializeGlobals();
    static public native void finalizeGlobals();
    static public native boolean isInitialized();

    static public native String version();
    static public native String buildDate();
}
