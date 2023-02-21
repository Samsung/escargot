package com.samsung.lwe.escargot;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

@RunWith(AndroidJUnit4.class)
public class EscargotTest {
    @Test
    public void initTest() {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        assertTrue(vmInstance.hasValidNativePointer());
        Context context = Context.create(vmInstance);
        assertTrue(context.hasValidNativePointer());

        context.destroy();
        assertFalse(context.hasValidNativePointer());
        vmInstance.destroy();
        assertFalse(vmInstance.hasValidNativePointer());
        Globals.finalizeGlobals();
    }

    @Test
    public void initMultipleTimesTest() {
        // pass if there is no crash
        Globals.initializeGlobals();
        Globals.finalizeGlobals();

        Globals.initializeGlobals();
        Globals.finalizeGlobals();

        Globals.initializeGlobals();
        Globals.finalizeGlobals();
    }

    @Test
    public void muitipleVMTest() {
        Globals.initializeGlobals();
        // pass if there is no crash
        // user can create multiple VM
        VMInstance.create(Optional.empty(), Optional.empty());
        VMInstance.create(Optional.of("en-US"), Optional.empty());
        VMInstance.create(Optional.empty(), Optional.of("Asia/Seoul"));
        Globals.finalizeGlobals();
    }

    @Test
    public void muitipleContextTest() {
        Globals.initializeGlobals();
        // pass if there is no crash
        // user can create multiple context
        VMInstance vm = VMInstance.create(Optional.empty(), Optional.empty());
        Context.create(vm);
        Context.create(vm);
        Context.create(vm);
        Globals.finalizeGlobals();
    }

    @Test
    public void simpleScriptRunTest() {
        Globals.initializeGlobals();

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        assertTrue(vmInstance.hasValidNativePointer());
        Context context = Context.create(vmInstance);
        assertTrue(context.hasValidNativePointer());

        // test script parsing error
        assertFalse(Evaluator.evalScript(context, "@@", "invalid", true).isPresent());
        // test runtime error
        assertFalse(Evaluator.evalScript(context, "throw new Error()", "invalid", true).isPresent());

        assertTrue(Evaluator.evalScript(context, "a = 1", "from_java.js", true).get().equals("1"));
        assertTrue(Evaluator.evalScript(context, "a", "from_java2.js", true).get().equals("1"));

        Globals.finalizeGlobals();
    }

    @Test
    public void attachICUTest() {
        Globals.initializeGlobals();

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        assertTrue(Evaluator.evalScript(context, "a = new Date('2000/1/1')", "from_java3.js", true).get().contains("2000"));
        assertTrue(Evaluator.evalScript(context,
                "const koDtf = new Intl.DateTimeFormat(\"ko\", { dateStyle: \"long\" }); koDtf.format(a)", "from_java4.js", true).get().contains("2000"));

        Globals.finalizeGlobals();
    }

    @Test
    public void bridgeTest() {
        Globals.initializeGlobals();

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        class TestBridge extends Bridge.Adapter {
            public boolean called = false;
            @Override
            public Optional<String> callback(Optional<String> data) {
                assertFalse(called);
                assertTrue(data.get().equals("dddd"));
                called = true;
                return Optional.of(data.get() + "ASdfasdfasdf");
            }
        };
        TestBridge testBridge = new TestBridge();
        Bridge.register(context, "Native", "addString", testBridge);
        Bridge.register(context, "Native", "returnString", new Bridge.Adapter() {
            @Override
            public Optional<String> callback(Optional<String> data) {
                assertFalse(data.isPresent());
                return Optional.of("string from java");
            }
        });
        Bridge.register(context, "Native", "returnNothing", new Bridge.Adapter() {
            @Override
            public Optional<String> callback(Optional<String> data) {
                return Optional.empty();
            }
        });

        assertTrue(Evaluator.evalScript(context, "Native.addString('dddd')", "from_java5.js", true).get().equals("ddddASdfasdfasdf"));
        assertTrue(testBridge.called);

        assertTrue(Evaluator.evalScript(context, "Native.returnString()", "from_java6.js", true).get().equals("string from java"));
        assertTrue(Evaluator.evalScript(context, "Native.returnNothing() === undefined", "from_java7.js", true).get().equals("true"));

        context.destroy();
        vmInstance.destroy();
        Globals.finalizeGlobals();
    }

    @Test
    public void nonHeapValueTest() {
        Globals.initializeGlobals();

        Value v = Value.createUndefined();
        assertTrue(v.isUndefined());
        assertFalse(v.isNull());
        assertFalse(v.isNumber());

        v = Value.createNull();
        assertTrue(v.isNull());
        assertFalse(v.isUndefined());
        assertFalse(v.isNumber());

        v = Value.create(123);
        assertTrue(v.isInt32());
        assertEquals(v.asInt32(), 123);
        assertTrue(v.isNumber());
        assertEquals(v.asNumber(), 123.0, 0);

        v = Value.create(true);
        assertTrue(v.isTrue());
        assertTrue(v.isBoolean());
        assertTrue(v.asBoolean());
        assertFalse(v.isInt32());
        assertFalse(v.isUndefined());

        v = Value.create(false);
        assertTrue(v.isFalse());
        assertTrue(v.isBoolean());
        assertFalse(v.asBoolean());
        assertFalse(v.isInt32());
        assertFalse(v.isUndefined());

        Globals.finalizeGlobals();
    }

    @Test
    public void heapValueTest() {
        Globals.initializeGlobals();

        Value v = Value.create(3.14);
        assertTrue(v.isNumber());
        assertEquals(v.asNumber(), 3.14, 0);
        assertFalse(v.isInt32());

        Globals.finalizeGlobals();
    }
}