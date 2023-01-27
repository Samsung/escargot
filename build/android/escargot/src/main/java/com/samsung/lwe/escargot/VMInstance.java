package com.samsung.lwe.escargot;

import java.util.Optional;

public class VMInstance extends NativePointerHolder {
    // you can to provide timezone as TZ database name like "US/Pacific".
    // if you don't provide, we try to detect system timezone.
    public native static VMInstance create(Optional<String> locale, Optional<String> timezone);
    native protected void releaseNativePointer();

    @Override
    public void destroy() {
        releaseNativePointer();
    }
}