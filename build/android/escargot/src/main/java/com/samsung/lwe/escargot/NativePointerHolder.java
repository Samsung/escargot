package com.samsung.lwe.escargot;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;

public abstract class NativePointerHolder {
    static ThreadLocal<ReferenceQueue<Object>> s_referenceQueue = ThreadLocal.withInitial(() -> new ReferenceQueue<>());
    static ThreadLocal<Set<NativePointerHolderPhantomReference>> s_referenceSet = ThreadLocal.withInitial(() -> new HashSet<>());
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

    NativePointerHolder(long nativePointer, boolean isHeapValue)
    {
        m_nativePointer = nativePointer;
        if (isHeapValue) {
            s_referenceSet.get().add(new NativePointerHolderPhantomReference(this, s_referenceQueue.get()));
        }
    }

    public static void cleanUp()
    {
        Method refersToMethod = null;
        Method[] methods = NativePointerHolderPhantomReference.class
                .getSuperclass().getSuperclass() // <- points Reference<T>
                .getDeclaredMethods();
        for (int i = 0; i < methods.length; i ++) {
            if (methods[i].getName().equals("refersTo")) {
                refersToMethod = methods[i];
                break;
            }
        }

        for (NativePointerHolderPhantomReference reference : s_referenceSet.get()) {
            if (refersToMethod != null) {
                try {
                    refersToMethod.invoke(reference, (java.lang.Object)null);
                    continue;
                } catch (Exception e) {
                    refersToMethod = null;
                    e.printStackTrace();
                }
            }
            reference.isEnqueued();
        }

        Reference<?> ref;
        while ((ref = s_referenceQueue.get().poll()) != null) {
            s_referenceSet.get().remove(((NativePointerHolderPhantomReference)ref));
            ((NativePointerHolderPhantomReference)ref).release();
            ref.clear();
        }
    }

    static private native void releaseNativePointerMemory(long pointer);
    protected long m_nativePointer;
}
