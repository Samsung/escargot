package com.samsung.lwe.escargot;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.ref.ReferenceQueue;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(AndroidJUnit4.class)
public class NativePointerHolderTest {
    @Test
    public void finalizeThreadReleasesLiveAndQueuedHolders() throws InterruptedException {
        Globals.initializeGlobals();
        try {
            VMInstance mainVM = VMInstance.create(Optional.empty(), Optional.empty());
            Set<NativePointerHolder.NativePointerHolderPhantomReference> mainReferences =
                    NativePointerHolder.s_referenceSet.get();
            int mainReferenceCount = mainReferences.size();
            AtomicReference<Throwable> failure = new AtomicReference<>();
            for (int session = 0; session < 3; session++) {
                Thread worker = new Thread(() -> {
                    try {
                        // Also cover reusing the same thread after finalization.
                        for (int iteration = 0; iteration < 2; iteration++) {
                            Globals.initializeThread();
                            try {
                                NativePointerHolder.cleanUp();
                                assertTrue(NativePointerHolder.s_referenceSet.get().isEmpty());
                                VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
                                Context context = Context.create(vm);
                                JavaScriptString value = JavaScriptString.create("still reachable");
                                Set<NativePointerHolder.NativePointerHolderPhantomReference> references =
                                        NativePointerHolder.s_referenceSet.get();
                                ReferenceQueue<Object> queue = NativePointerHolder.s_referenceQueue.get();
                                assertEquals(3, references.size());
                                NativePointerHolder.NativePointerHolderPhantomReference[] snapshot =
                                        references.toArray(new NativePointerHolder.NativePointerHolderPhantomReference[0]);
                                // A queued reference also belongs to the set. It must
                                // be released once and removed from both containers.
                                assertTrue(references.iterator().next().enqueue());

                                Globals.finalizeThread();

                                assertFalse(Globals.isInitialized());
                                assertTrue(references.isEmpty());
                                assertNull(queue.poll());
                                for (NativePointerHolder.NativePointerHolderPhantomReference reference : snapshot) {
                                    assertEquals(0, reference.m_nativePointer);
                                    // A reference pending in the Java GC can arrive
                                    // late, after shutdown has drained the queue.
                                    reference.enqueue();
                                }
                                // Keep the Java wrappers live through shutdown.
                                assertNotNull(vm);
                                assertNotNull(context);
                                assertNotNull(value);
                                Globals.finalizeThread();
                            } finally {
                                Globals.finalizeThread();
                            }
                        }
                    } catch (Throwable error) {
                        failure.set(error);
                    }
                });
                worker.start();
                worker.join();
                if (failure.get() != null) {
                    throw new AssertionError("Worker finalization failed", failure.get());
                }
                assertSame(mainReferences, NativePointerHolder.s_referenceSet.get());
                assertEquals(mainReferenceCount, mainReferences.size());
                // Finalizing a worker must leave another thread's VM usable.
                Context mainContext = Context.create(mainVM);
                assertNotNull(mainContext.getGlobalObject());
                mainReferenceCount = mainReferences.size();
            }
        } finally {
            Globals.finalizeGlobals();
        }
    }

    @Test
    public void finalizeGlobalsReleasesLiveHolders() {
        Globals.initializeGlobals();
        try {
            VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
            Context context = Context.create(vm);
            Set<NativePointerHolder.NativePointerHolderPhantomReference> references =
                    NativePointerHolder.s_referenceSet.get();
            ReferenceQueue<Object> queue = NativePointerHolder.s_referenceQueue.get();
            assertFalse(references.isEmpty());

            Globals.finalizeGlobals();

            assertTrue(references.isEmpty());
            assertNull(queue.poll());
            assertNotNull(vm);
            assertNotNull(context);
        } finally {
            Globals.finalizeGlobals();
        }
    }
    @Test
    public void cleanupReleasesOnlyQueuedAlias() {
        Globals.initializeGlobals();
        try {
            VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
            Context context = Context.create(vm);
            JavaScriptObject original = JavaScriptObject.create(context);
            assertTrue(original.set(context, JavaScriptString.create("value"), JavaScriptValue.create(42)).get());
            assertTrue(context.getGlobalObject().set(context, JavaScriptString.create("shared"), original).get());
            JavaScriptValue alias = context.getGlobalObject().get(context, JavaScriptString.create("shared")).get();
            assertTrue(original.equalsTo(context, alias).get());
            assertTrue(original.m_nativePointer != alias.m_nativePointer);
            NativePointerHolder.NativePointerHolderPhantomReference queued = null;
            for (NativePointerHolder.NativePointerHolderPhantomReference reference : NativePointerHolder.s_referenceSet.get()) {
                if (reference.m_nativePointer == alias.m_nativePointer) {
                    queued = reference;
                    break;
                }
            }
            assertNotNull(queued);
            // Simulate a queued wrapper deterministically. Do not use alias
            // again after its native root has been explicitly released.
            assertTrue(queued.enqueue());
            NativePointerHolder.cleanUp();
            assertEquals(0, queued.m_nativePointer);
            assertFalse(NativePointerHolder.s_referenceSet.get().contains(queued));
            NativePointerHolder.cleanUp();
            queued.release();
            Memory.gc();
            assertEquals(42, original.get(context, JavaScriptString.create("value")).get().asInt32());
            assertTrue(context.getGlobalObject().get(context, JavaScriptString.create("shared"))
                    .get().equalsTo(context, original).get());
        } finally {
            Globals.finalizeGlobals();
        }
    }

    @Test
    public void liveObjectsSurviveRepeatedJavaAndNativeGC() {
        Globals.initializeGlobals();
        try {
            VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
            Context context = Context.create(vm);
            // The IIFE leaves no JS global owning the result. Its Java wrapper
            // must retain the typed array and its backing store by itself.
            JavaScriptObject bytes = Evaluator.evalScript(context,
                    "(() => { const b = new Uint8Array(1024 * 1024); "
                    + "b[0] = 17; b[b.length - 1] = 91; return b; })()", "retained-buffer.js")
                    .get().asScriptObject();
            JavaScriptString first = JavaScriptString.create("0");
            JavaScriptString last = JavaScriptString.create("1048575");
            JavaScriptString length = JavaScriptString.create("length");
            for (int i = 0; i < 25; i++) {
                System.gc();
                NativePointerHolder.cleanUp();
                Memory.gc();
                assertEquals(17, bytes.get(context, first).get().asInt32());
                assertEquals(91, bytes.get(context, last).get().asInt32());
                assertEquals(1024 * 1024, bytes.get(context, length).get().asInt32());
            }
        } finally {
            Globals.finalizeGlobals();
        }
    }
}
