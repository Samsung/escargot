package com.samsung.lwe.escargot;

import static org.junit.Assert.assertEquals;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

// Minimal on-device smoke test for the released AAR -- kept deliberately
// separate from EscargotTest.java's full suite so android-release.yml can
// run just this class (a few seconds) instead of the whole ~5-10 minute
// instrumented suite on every tag push. Mirrors
// tools/release-smoketest/Smoke.java's two checks (arithmetic + Date), which
// exist for the host jars (escargot-ubuntu/escargot-mac); this is the AAR's
// equivalent, since the AAR's native libs are NDK-compiled for Android ABIs
// and can't be exercised by a plain `java` CLI the way a host jar can.
@RunWith(AndroidJUnit4.class)
public class ReleaseSmokeTest {
    @Test
    public void arithmetic() {
        Globals.initializeGlobals();
        Context context = Context.create(VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul")));

        String sum = Evaluator.evalScript(context, "1 + 2", "smoke-arith.js", true)
                .get().toString(context).get().toJavaString();
        assertEquals("3", sum);

        context = null;
        Globals.finalizeGlobals();
    }

    @Test
    public void date() {
        Globals.initializeGlobals();
        Context context = Context.create(VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul")));

        String year = Evaluator.evalScript(context, "new Date(2000, 0, 1).getFullYear().toString()", "smoke-date.js", true)
                .get().toString(context).get().toJavaString();
        assertEquals("2000", year);

        context = null;
        Globals.finalizeGlobals();
    }
}
