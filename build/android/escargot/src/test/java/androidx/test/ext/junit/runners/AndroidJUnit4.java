package androidx.test.ext.junit.runners;

import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.InitializationError;

// Run the shared Android tests on a host JVM with standard JUnit semantics,
// including assumptions, fixtures and per-test failure reporting.
public class AndroidJUnit4 extends BlockJUnit4ClassRunner {
    public AndroidJUnit4(Class<?> testClass) throws InitializationError {
        super(testClass);
    }
}
