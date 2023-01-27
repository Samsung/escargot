package com.samsung.lwe.escargot;

public abstract class NativePointerHolder {
    abstract public void destroy();
    public boolean hasValidNativePointer()
    {
        return m_nativePointer != 0;
    }
    protected void finalize()
    {
        destroy();
    }

    protected long m_nativePointer;
}
