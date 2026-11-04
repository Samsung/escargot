package com.samsung.lwe.escargot;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(AndroidJUnit4.class)
public class NativePointerHolderMemoryTest {
    private static final int WARMUP_SESSIONS = 3;
    private static final int MEASURED_SESSIONS = 12;
    private static final long MIB = 1024 * 1024;
    // Allow allocator/JVM noise, while detecting retention of the 32 MiB
    // backing store per session well before the process risks running out.
    private static final long MAX_RETAINED_RSS = 48 * MIB;
    private static final String WORKLOAD =
            "var sessionBytes = new Uint8Array(32 * 1024 * 1024);"
            + "for (var i = 0; i < sessionBytes.length; i += 4096) sessionBytes[i] = 1;"
            + "var sessionObjects = new Array(100000);"
            + "for (var i = 0; i < sessionObjects.length; i++)"
            + "  sessionObjects[i] = {id: i, text: 'session value ' + i};"
            + "sessionObjects.length + sessionBytes.length;";

    @Test
    public void repeatedThreadSessionsDoNotAccumulateNativeMemory() throws Exception {
        assumeTrue("RSS measurement requires /proc/self/status",
                new File("/proc/self/status").canRead());
        Globals.initializeGlobals();
        try {
            for (int session = 0; session < WARMUP_SESSIONS; session++) {
                runSession();
            }
            long baselineRss = residentBytes();
            StringBuilder samples = new StringBuilder("baseline RSS=" + baselineRss);
            System.out.println("Escargot thread memory: " + samples);
            for (int session = 0; session < MEASURED_SESSIONS; session++) {
                long[] heap = runSession();
                long rss = residentBytes();
                samples.append("\nsession ").append(session + 1)
                        .append(": RSS=").append(rss)
                        .append(", native GC heap before/after finalizeThread=")
                        .append(heap[0]).append('/').append(heap[1]);
                System.out.println("Escargot thread memory: " + samples.substring(samples.lastIndexOf("\n") + 1));
                assertTrue("Native memory accumulated across completed threads:\n" + samples,
                        rss - baselineRss <= MAX_RETAINED_RSS);
            }
        } finally {
            Globals.finalizeGlobals();
        }
    }

    private static long[] runSession() throws InterruptedException {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        long[] heap = new long[2];
        Thread worker = new Thread(() -> {
            try {
                Globals.initializeThread();
                try {
                    VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
                    Context context = Context.create(vm);
                    JavaScriptValue result = Evaluator.evalScript(
                            context, WORKLOAD, "thread-memory.js", false).get();
                    assertEquals(32 * 1024 * 1024 + 100000, result.asInt32());
                    heap[0] = Memory.heapSize();
                    // Do not invoke System.gc(), Memory.gc() or cleanUp(): the
                    // shutdown path itself must release even live wrappers.
                    Globals.finalizeThread();
                    heap[1] = Memory.heapSize();
                    assertTrue(vm.m_nativePointer != 0 && context.m_nativePointer != 0
                            && result.m_nativePointer != 0);
                } finally {
                    Globals.finalizeThread();
                }
                assertFalse(Globals.isInitialized());
            } catch (Throwable error) {
                failure.set(error);
            }
        }, "escargot-memory-session");
        worker.start();
        worker.join();
        if (failure.get() != null) {
            throw new AssertionError("Session failed", failure.get());
        }
        return heap;
    }

    // /proc measures process RSS, including the native GC's mmap allocations
    // that Android's malloc-only native heap counter does not cover.
    private static long residentBytes() throws IOException {
        try (BufferedReader reader = new BufferedReader(new FileReader("/proc/self/status"))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.startsWith("VmRSS:")) {
                    return Long.parseLong(line.trim().split("\\s+")[1]) * 1024;
                }
            }
        }
        throw new IOException("VmRSS missing from /proc/self/status");
    }
}
