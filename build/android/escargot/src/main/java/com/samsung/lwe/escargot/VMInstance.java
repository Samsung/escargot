package com.samsung.lwe.escargot;

import java.util.Optional;

public class VMInstance extends NativePointerHolder {
    protected VMInstance(long nativePointer)
    {
        super(nativePointer);
    }
    // you can to provide timezone as TZ database name like "US/Pacific".
    // if you don't provide, we try to detect system timezone.
    public native static VMInstance create(Optional<String> locale, Optional<String> timezone);

    public native boolean hasPendingJob();
    public native void executePendingJob();
    public void executeEveryPendingJobIfExists()
    {
        while (hasPendingJob()) {
            executePendingJob();
        }
    }
}