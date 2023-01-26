package com.samsung.lwe.escargot;

public class Globals {
    static { Escargot.init(); }
    // you can call initializeGlobals function just once within single process!
    static public native void initializeGlobals();
    static public native void finalizeGlobals();

    static public native String version();
    static public native String buildDate();
}
