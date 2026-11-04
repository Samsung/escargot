package com.samsung.lwe.escargot;

final public class Globals {
    private Globals() {

    }
    static { Escargot.init(); }

    static public native void initializeGlobals();
    /** Releases this thread's native holders and shuts down the runtime.
     * All wrappers created on this thread become invalid. Finalize worker
     * threads on their owning threads before calling this method.
     */
    static public native void finalizeGlobals();
    static public native void initializeThread();
    /** Releases all native holders created on the current thread before
     * shutting it down. Its VMInstance, Context and JavaScriptValue wrappers
     * must not be used afterwards, even if they are still reachable in Java.
     */
    static public native void finalizeThread();

    static public native boolean isInitialized();

    static public native String version();
    static public native String buildDate();
}
