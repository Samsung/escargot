package com.samsung.lwe.escargot;

public class Context extends NativePointerHolder {
    public native static Context create(VMInstance vmInstance);
    native protected void releaseNativePointer();

    @Override
    public void destroy() {
        releaseNativePointer();
    }
}
