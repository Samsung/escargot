package com.samsung.lwe.escargot;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.util.ArrayList;
import java.util.List;

public abstract class NativePointerHolder {
    static ThreadLocal<ReferenceQueue<Object>> s_referenceQueue = ThreadLocal.withInitial(() -> new ReferenceQueue<>());
    static ThreadLocal<List<NativePointerHolderPhantomReference>> s_referenceList = ThreadLocal.withInitial(() -> new ArrayList<>());
    static { Escargot.init(); }

    public static class NativePointerHolderPhantomReference extends PhantomReference<Object> {
        public NativePointerHolderPhantomReference(NativePointerHolder nativePointerHolder, ReferenceQueue<Object> queue)
        {
            super(nativePointerHolder, queue);
            m_nativePointer = nativePointerHolder.m_nativePointer;
        }
        public void release()
        {
            releaseNativePointerMemory(m_nativePointer);
        }
        long m_nativePointer;
    }

    NativePointerHolder(long nativePointer)
    {
        m_nativePointer = nativePointer;
        s_referenceList.get().add(new NativePointerHolderPhantomReference(this, s_referenceQueue.get()));
    }

    public static void cleanUp()
    {
        try {
            for (PhantomReference<Object> reference : s_referenceList.get()) {
                reference.refersTo(null);
            }
        } catch (NoSuchMethodError e) {
            for (PhantomReference<Object> reference : s_referenceList.get()) {
                reference.isEnqueued();
            }
        }

        Reference<?> ref;
        while ((ref = s_referenceQueue.get().poll()) != null) {
            ((NativePointerHolderPhantomReference)ref).release();
            ref.clear();
        }
    }

    static private native void releaseNativePointerMemory(long pointer);
    protected long m_nativePointer;
}
