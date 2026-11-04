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
            if (m_nativePointer != 0) {
                releaseNativePointerMemory(m_nativePointer);
                m_nativePointer = 0;
            }
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

    // Called on the owning thread before its native heap is finalized. Java
    // wrappers may still be reachable, but must not be used after finalization.
    static void releaseAll()
    {
        for (NativePointerHolderPhantomReference reference : s_referenceSet.get()) {
            reference.release();
            reference.clear();
        }
        s_referenceSet.get().clear();

        Reference<?> reference;
        while ((reference = s_referenceQueue.get().poll()) != null) {
            reference.clear();
        }
    }

    static private native void releaseNativePointerMemory(long pointer);
    protected long m_nativePointer;
}
