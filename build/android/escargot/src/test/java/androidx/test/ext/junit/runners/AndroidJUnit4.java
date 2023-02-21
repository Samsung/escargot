package androidx.test.ext.junit.runners;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.notification.RunNotifier;

import java.lang.reflect.Method;

public class AndroidJUnit4 extends Runner {
    private Class testClass;
    public AndroidJUnit4(Class testClass) {
        super();
        this.testClass = testClass;
    }

    @Override
    public Description getDescription() {
        return Description
          .createTestDescription(testClass, "AndroidJUnit4(mock) description");
    }

    @Override
    public void run(RunNotifier notifier) {
        System.out.println("running the tests from AndroidJUnit4(mock): " + testClass);
        try {
            Object testObject = testClass.newInstance();
            for (Method method : testClass.getMethods()) {
                if (method.isAnnotationPresent(Test.class)) {
                    notifier.fireTestStarted(Description
                      .createTestDescription(testClass, method.getName()));
                    method.invoke(testObject);
                    notifier.fireTestFinished(Description
                      .createTestDescription(testClass, method.getName()));
                }
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
