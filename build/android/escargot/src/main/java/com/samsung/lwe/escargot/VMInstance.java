package com.samsung.lwe.escargot;

import java.util.Optional;

public class VMInstance {
    // you can to provide timezone as TZ database name like "US/Pacific".
    // if you don't provide, we try to detect system timezone.
    public native static VMInstance create(Optional<String> locale, Optional<String> timezone);
    public native void destroy();
    public boolean hasValidNativePointer()
    {
        return m_nativePointer != 0;
    }

    private long m_nativePointer;
}
