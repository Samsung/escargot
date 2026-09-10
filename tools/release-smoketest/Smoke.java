// Standalone smoke test for the escargot-ubuntu/escargot-mac host jars built
// by .github/workflows/android-release.yml. Deliberately compiled and run
// with ONLY the jar under test on the classpath, from a directory unrelated
// to wherever it was built -- catches packaging bugs (e.g. a native
// dependency the Jar task forgot to bundle) that testDebugUnitTest can't,
// since that task runs in-process against the build dir's own natively
// compiled libs, not the jar's bundled copy. See RELEASE.md's
// "Before trusting the jar, smoke-test it standalone" section.
import com.samsung.lwe.escargot.*;
import java.util.Optional;

public class Smoke {
    public static void main(String[] args) {
        Globals.initializeGlobals();
        Context context = Context.create(VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul")));

        String sum = Evaluator.evalScript(context, "1 + 2", "smoke-arith.js", true)
                .get().toString(context).get().toJavaString();
        if (!sum.equals("3")) {
            throw new RuntimeException("smoke test failed: 1 + 2 -> " + sum + " (expected 3)");
        }

        String year = Evaluator.evalScript(context, "new Date(2000, 0, 1).getFullYear().toString()", "smoke-date.js", true)
                .get().toString(context).get().toJavaString();
        if (!year.equals("2000")) {
            throw new RuntimeException("smoke test failed: new Date(2000, 0, 1).getFullYear() -> " + year + " (expected 2000)");
        }

        System.out.println("smoke test OK: 1 + 2 = " + sum + ", new Date(2000, 0, 1).getFullYear() = " + year);
    }
}
