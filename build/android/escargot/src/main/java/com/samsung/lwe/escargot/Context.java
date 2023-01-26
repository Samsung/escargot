package com.samsung.lwe.escargot;

public class Context {
    public native static Context create(VMInstance vmInstance);
    public native void destroy();
    public boolean hasValidNativePointer()
    {
        return m_nativePointer != 0;
    }

    private long m_nativePointer;
}
