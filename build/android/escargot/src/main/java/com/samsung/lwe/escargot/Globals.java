package com.samsung.lwe.escargot;

final public class Globals {
    private Globals() {

    }
    static { Escargot.init(); }

    static public native void initializeGlobals();
    static public native void finalizeGlobals();
    static public native void initializeThread();
    static public native void finalizeThread();

    static public native boolean isInitialized();

    static public native String version();
    static public native String buildDate();
}
