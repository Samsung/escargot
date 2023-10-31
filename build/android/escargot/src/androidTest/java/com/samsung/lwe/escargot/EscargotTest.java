package com.samsung.lwe.escargot;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.samsung.lwe.escargot.util.MultiThreadExecutor;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Optional;

@RunWith(AndroidJUnit4.class)
public class EscargotTest {
    public void printNegativeTC(String desc) {
        System.out.println("[NEGATIVE TC] " + desc);
    }

    public void printPositiveTC(String desc) {
        System.out.println("[POSITIVE TC] " + desc);
    }


    @Test
    public void initTest() {
        Globals.initializeGlobals();
        // null test
        assertThrows(NullPointerException.class, () -> {
            printNegativeTC("VMInstance create with null 1");
            VMInstance.create(Optional.empty(), null);
        });
        assertThrows(NullPointerException.class, () -> {
            printNegativeTC("VMInstance create with null 2");
            VMInstance.create(null, Optional.empty());
        });
        assertThrows(NullPointerException.class, () -> {
            printNegativeTC("Context create with null 2");
            Context.create(null);
        });

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        // test writable of /tmp/
        Optional<String> s = Escargot.copyStreamAsTempFile(new ByteArrayInputStream("asdf".getBytes()), "test", ".txt", false);
        assertTrue(s.isPresent());

        printPositiveTC("test build config and copy file in jar");

        for (int i = 0; i < 30000; i++) {
            // alloc many trash objects for testing memory management
            JavaScriptString.create("asdf");
        }

        context = null;
        vmInstance = null;
        Globals.finalizeGlobals();

        printPositiveTC("VMInstance, Context create");
    }

    @Test
    public void initMultipleTimesTest() {
        // pass if there is no crash
        Globals.initializeGlobals();
        Globals.initializeGlobals();
        Globals.initializeGlobals();
        Globals.finalizeGlobals();
        Globals.finalizeGlobals();
        Globals.finalizeGlobals();
        Globals.finalizeGlobals();
        Globals.finalizeGlobals();

        Globals.initializeGlobals();
        Globals.finalizeGlobals();

        Globals.initializeGlobals();
        Globals.finalizeGlobals();

        printNegativeTC("Globals inbalance call test");
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

        printPositiveTC("muitipleVMTest");
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

        printPositiveTC("muitipleContextTest");
    }

    @Test
    public void simpleScriptRunTest() {
        Globals.initializeGlobals();

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        assertThrows(NullPointerException.class, () -> {
            Evaluator.evalScript(null, "", null, true);
            printNegativeTC("Evaluator.evalScript with null");
        });

        // pass if not assert doesn't throws
        Evaluator.evalScript(context, null, null, true);
        printNegativeTC("Evaluator.evalScript with null 2");

        JavaScriptValue vv = JavaScriptString.create(System.getProperty("java.version"));
        context.getGlobalObject().set(context, JavaScriptString.create("ddd"), vv);
        Evaluator.evalScript(context, "'java.version' + ddd", null, true);
        printPositiveTC("Evaluator.evalScript test 1");

        // test script parsing error
        assertFalse(Evaluator.evalScript(context, "@@", "invalid", true).isPresent());
        // test runtime error
        assertFalse(Evaluator.evalScript(context, "throw new Error()", "invalid", true).isPresent());
        printPositiveTC("Evaluator.evalScript test 2");

        assertTrue(Evaluator.evalScript(context, "a = 1", "from_java.js", true).get().toString(context).get().toJavaString().equals("1"));
        assertTrue(Evaluator.evalScript(context, "a", "from_java2.js", true).get().toString(context).get().toJavaString().equals("1"));

        context = null;
        vmInstance = null;
        Globals.finalizeGlobals();

        printPositiveTC("Evaluator.evalScript test 3");
    }

    @Test
    public void attachICUTest() {
        Globals.initializeGlobals();

        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        assertTrue(Evaluator.evalScript(context, "a = new Date('2000/1/1')", "from_java3.js", true).get().toString(context).get().toJavaString().contains("2000"));
        assertTrue(Evaluator.evalScript(context,
                "const koDtf = new Intl.DateTimeFormat(\"ko\", { dateStyle: \"long\" }); koDtf.format(a)", "from_java4.js", true).get().toString(context).get().toJavaString().contains("2000"));

        context = null;
        vmInstance = null;
        Globals.finalizeGlobals();

        printPositiveTC("attachICUTest");
    }

    @Test
    public void simpleOneByOneThreadingTest() {
        Thread t1 = new Thread(() -> {
            assertFalse(Globals.isInitialized());
            Globals.initializeGlobals();

            VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
            Context context = Context.create(vmInstance);

            assertEquals(JavaScriptValue.create(123).toString(context).get().toJavaString(), "123");
            assertEquals(JavaScriptValue.create(123).toBoolean(context).get(), true);

            JavaScriptValue vv = JavaScriptString.create(System.getProperty("java.version"));
            context.getGlobalObject().set(context, JavaScriptString.create("ddd"), vv);
            Evaluator.evalScript(context, "'java.version' + ddd", "invalid", true);

            for (int i = 0; i < 30000; i++) {
                // alloc many trash objects for testing memory management
                JavaScriptString.create("asdf");
            }

            context = null;
            vmInstance = null;
            System.gc();
            Memory.gc();
            Globals.finalizeGlobals();
        });
        t1.start();
        try {
            t1.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        Thread t2 = new Thread(() -> {
            assertFalse(Globals.isInitialized());
            Globals.initializeGlobals();

            VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
            Context context = Context.create(vmInstance);

            String testString = "[1, 2, 3]";
            JavaScriptValue result = context.getGlobalObject().jsonParse(context, JavaScriptString.create(testString)).get();
            assertTrue(result.isArrayObject());

            JavaScriptValue vv = JavaScriptString.create(System.getProperty("java.version"));
            context.getGlobalObject().set(context, JavaScriptString.create("ddd"), vv);
            Evaluator.evalScript(context, "'java.version' + ddd", "invalid", true);

            context = null;
            vmInstance = null;
            System.gc();
            Memory.gc();
            Globals.finalizeGlobals();
        });

        t2.start();
        try {
            t2.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        printPositiveTC("simpleOneByOneThreadingTest");
    }

    @Test
    public void simpleMultiThreadingTest() {
        Globals.initializeGlobals();

        Thread t1 = new Thread(() -> {
            assertFalse(Globals.isInitialized());
            Globals.initializeThread();

            VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
            Context context = Context.create(vmInstance);

            assertEquals(JavaScriptValue.create(123).toString(context).get().toJavaString(), "123");
            assertEquals(JavaScriptValue.create(123).toBoolean(context).get(), true);

            JavaScriptValue vv = JavaScriptString.create(System.getProperty("java.version"));
            context.getGlobalObject().set(context, JavaScriptString.create("ddd"), vv);
            Evaluator.evalScript(context, "'java.version' + ddd", "invalid", true);

            for (int i = 0; i < 999999; i++) {
                // alloc many trash objects for testing memory management
                JavaScriptString.create("asdf");
            }

            context = null;
            vmInstance = null;
            System.gc();
            Memory.gc();
            Globals.finalizeThread();
        });

        Thread t2 = new Thread(() -> {
            assertFalse(Globals.isInitialized());
            Globals.initializeThread();

            VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
            Context context = Context.create(vmInstance);

            String testString = "[1, 2, 3]";
            JavaScriptValue result = context.getGlobalObject().jsonParse(context, JavaScriptString.create(testString)).get();
            assertTrue(result.isArrayObject());

            JavaScriptValue vv = JavaScriptString.create(System.getProperty("java.version"));
            context.getGlobalObject().set(context, JavaScriptString.create("ddd"), vv);
            Evaluator.evalScript(context, "'java.version' + ddd", "invalid", true);

            for (int i = 0; i < 999999; i++) {
                // alloc many trash objects for testing memory management
                JavaScriptString.create("asdf");
            }

            context = null;
            vmInstance = null;
            System.gc();
            Memory.gc();
            Globals.finalizeThread();
        });

        t1.start();
        t2.start();

        try {
            t1.join();
            t2.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        assertFalse(t1.isAlive());
        assertFalse(t2.isAlive());

        System.gc();
        Memory.gc();
        Globals.finalizeGlobals();

        printPositiveTC("simpleMultiThreadingTest");
    }

    private Context initEngineAndCreateContext() {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        return Context.create(vmInstance);
    }

    private void finalizeEngine() {
        System.gc();
        System.gc();
        System.gc();
        Memory.gc();
        Memory.gc();
        Globals.finalizeGlobals();
    }

    @Test
    public void bridgeTest() {
        Context context = initEngineAndCreateContext();

        class EmptyBridge extends Bridge.Adapter {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                return Optional.empty();
            }
        }

        assertThrows(NullPointerException.class, () -> {
            Bridge.register(null, "a", "b", new EmptyBridge());
        });
        printNegativeTC("register bridge with null 1");
        assertFalse(Bridge.register(context, null, "b", new EmptyBridge()));
        printNegativeTC("register bridge with null 2");
        assertFalse(Bridge.register(context, "", "b", new EmptyBridge()));
        printNegativeTC("register bridge with null 3");
        assertFalse(Bridge.register(context, "a", null, new EmptyBridge()));
        printNegativeTC("register bridge with null 4");
        assertFalse(Bridge.register(context, "a", "", new EmptyBridge()));
        printNegativeTC("register bridge with null 5");

        class TestBridge extends Bridge.Adapter {
            public boolean called = false;

            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                assertTrue(JavaScriptObject.create(context).isObject());
                assertFalse(called);
                assertTrue(data.get().isString());
                assertTrue(data.get().asScriptString().toJavaString().equals("dddd"));
                called = true;
                return Optional.of(JavaScriptString.create(data.get().asScriptString().toJavaString() + "ASdfasdfasdf"));
            }
        };

        TestBridge testBridge = new TestBridge();
        Bridge.register(context, "Native", "addString", testBridge);
        printPositiveTC("register bridge 1");
        Bridge.register(context, "Native", "returnString", new Bridge.Adapter() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                assertFalse(data.isPresent());
                return Optional.of(JavaScriptString.create("string from java"));
            }
        });
        printPositiveTC("register bridge 2");
        Bridge.register(context, "Native", "returnNothing", new Bridge.Adapter() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                return Optional.empty();
            }
        });

        assertTrue(Evaluator.evalScript(context, "Native.addString('dddd')", "from_java5.js", true).get().asScriptString().toJavaString().equals("ddddASdfasdfasdf"));
        assertTrue(testBridge.called);
        printPositiveTC("register bridge 3");

        assertTrue(Evaluator.evalScript(context, "Native.returnString()", "from_java6.js", true).get().asScriptString().toJavaString().equals("string from java"));
        printPositiveTC("register bridge 4");
        assertTrue(Evaluator.evalScript(context, "Native.returnNothing() === undefined", "from_java7.js", true).get().toString(context).get().toJavaString().equals("true"));
        printNegativeTC("register bridge return null");

        Bridge.register(context, "Native", "returnNull", new Bridge.Adapter() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                return null;
            }
        });

        assertTrue(Evaluator.evalScript(context, "Native.returnNull() === undefined", "from_java8.js", true).get().toString(context).get().toJavaString().equals("true"));
        printNegativeTC("register bridge return null 2");

        Bridge.register(context, "Native", "runtimeException", new Bridge.Adapter() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                throw new RuntimeException("test");
            }
        });
        {
            final Context finalContext = context;
            assertThrows(RuntimeException.class, () -> {
                Evaluator.evalScript(finalContext, "Native.runtimeException()", "from_java9.js", true);
            });
        }
        printNegativeTC("register bridge throws exception");

        context = null;
        finalizeEngine();
    }

    @Test
    public void nonHeapValueTest() {
        Context context = initEngineAndCreateContext();

        JavaScriptValue v = JavaScriptValue.createUndefined();
        assertTrue(v.isUndefined());
        assertFalse(v.isNull());
        assertFalse(v.isNumber());
        printPositiveTC("undefined value test");

        v = JavaScriptValue.createNull();
        assertTrue(v.isNull());
        assertFalse(v.isUndefined());
        assertFalse(v.isNumber());
        printPositiveTC("null value test");

        v = JavaScriptValue.create(123);
        assertTrue(v.isInt32());
        assertEquals(v.asInt32(), 123);
        assertTrue(v.isNumber());
        assertEquals(v.asNumber(), 123.0, 0);
        printPositiveTC("number value test");

        v = JavaScriptValue.create(true);
        assertTrue(v.isTrue());
        assertTrue(v.isBoolean());
        assertTrue(v.asBoolean());
        assertFalse(v.isInt32());
        assertFalse(v.isUndefined());

        v = JavaScriptValue.create(false);
        assertTrue(v.isFalse());
        assertTrue(v.isBoolean());
        assertFalse(v.asBoolean());
        assertFalse(v.isInt32());
        assertFalse(v.isUndefined());
        printPositiveTC("bool value test");

        context = null;
        finalizeEngine();
    }

    @Test
    public void heapValueTest() {
        Context context = initEngineAndCreateContext();

        JavaScriptValue v = JavaScriptValue.create(3.14);
        assertTrue(v.isNumber());
        assertEquals(v.asNumber(), 3.14, 0);
        assertFalse(v.isInt32());
        printPositiveTC("float value test");

        v = JavaScriptString.create((String) null);
        assertTrue(v.isString());
        assertTrue(v.asScriptString().toJavaString().equals(""));
        printNegativeTC("string with null");

        v = JavaScriptString.create("hello");
        assertTrue(v.isString());
        assertTrue(v.asScriptString().toJavaString().equals("hello"));
        assertFalse(v.isNumber());
        assertFalse(v.isUndefinedOrNull());
        printPositiveTC("string value test");

        context = null;
        finalizeEngine();
    }

    @Test
    public void valueConversionTest() throws InvocationTargetException, NoSuchMethodException, IllegalAccessException {
        Context context = initEngineAndCreateContext();

        ArrayList<String> methodList = new ArrayList<>();
        Method methods[] = JavaScriptValue.class.getMethods();
        for (int i = 0; i < methods.length; i ++) {
            if (methods[i].getName().startsWith("as")) {
                methodList.add(methods[i].getName());
            }
        }

        class TestSet {
            public JavaScriptValue value;
            public String[] okMethodNames;

            public TestSet(JavaScriptValue value, String[] okMethodNames)
            {
                this.value = value;
                this.okMethodNames = okMethodNames;
            }
        }
        TestSet ts[] = new TestSet[] {
            new TestSet(JavaScriptValue.createUndefined(), new String[]{}),
            new TestSet(JavaScriptValue.createNull(), new String[]{}),
            new TestSet(JavaScriptValue.create(true), new String[]{"asBoolean"}),
            new TestSet(JavaScriptValue.create(123), new String[]{"asInt32", "asNumber"}),
            new TestSet(JavaScriptValue.create(123.123), new String[]{"asNumber"}),
            new TestSet(JavaScriptString.create("asdf"), new String[]{"asScriptString"}),
            new TestSet(JavaScriptSymbol.create(Optional.empty()), new String[]{"asScriptSymbol"}),
            new TestSet(JavaScriptBigInt.create(123123), new String[]{"asScriptBigInt"}),
            new TestSet(JavaScriptObject.create(context), new String[]{"asScriptObject"}),
            new TestSet(JavaScriptArrayObject.create(context), new String[]{"asScriptObject", "asScriptArrayObject"}),
            new TestSet(JavaScriptPromiseObject.create(context), new String[]{"asScriptObject", "asScriptPromiseObject"}),
            new TestSet(JavaScriptErrorObject.create(context, JavaScriptErrorObject.ErrorKind.EvalError, "asdf"), new String[]{"asScriptObject", "asScriptErrorObject"}),
            new TestSet(JavaScriptJavaCallbackFunctionObject.create(context, "", 0, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                @Override
                public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                    return Optional.empty();
                }
            }), new String[]{"asScriptObject", "asScriptFunctionObject"})
        };

        for (int i = 0; i < ts.length; i ++) {
            TestSet t = ts[i];
            for (int j = 0; j < methodList.size(); j ++) {
                String name = methodList.get(j);
                boolean okName = false;
                for (int k = 0; k < t.okMethodNames.length; k ++) {
                    if (t.okMethodNames[k].equals(name)) {
                        okName = true;
                        break;
                    }
                }
                try {
                    Method method = JavaScriptValue.class.getMethod(name);
                    if (okName) {
                        // pass if there is no error
                        method.invoke(t.value);
                        printPositiveTC(name + " test");
                    } else {
                        try {
                            method.invoke(t.value);
                            assertTrue(false);
                        } catch (InvocationTargetException e) {
                            assertTrue(e.getCause() instanceof ClassCastException);
                        }
                        printNegativeTC(name + " throwing exception test");
                    }
                } catch (Exception e) {
                    throw e;
                }
            }
        }

        context = null;
        finalizeEngine();
    }

    @Test
    public void valueToStringTest() {
        Context context = initEngineAndCreateContext();

        Optional<JavaScriptString> result = JavaScriptValue.create(123).toString(context);
        assertTrue(result.isPresent());
        assertEquals(result.get().toJavaString(), "123");

        assertTrue(JavaScriptValue.create(Integer.MAX_VALUE).isInt32());
        assertTrue(JavaScriptValue.create(Integer.MAX_VALUE).isNumber());
        result = JavaScriptValue.create(Integer.MAX_VALUE).toString(context);
        assertTrue(result.isPresent());
        assertEquals(result.get().toJavaString(), Integer.MAX_VALUE + "");

        printPositiveTC("value toString test");

        context = null;
        finalizeEngine();
    }

    @Test
    public void valueOperationTest() {
        Context context = initEngineAndCreateContext();

        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toString(null);
        });
        printNegativeTC("value method null test 1");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toBoolean(null);
        });
        printNegativeTC("value method null test 2");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toInteger(null);
        });
        printNegativeTC("value method null test 3");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toInt32(null);
        });
        printNegativeTC("value method null test 4");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toNumber(null);
        });
        printNegativeTC("value method null test 5");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptValue.create(123).toObject(null);
        });
        printNegativeTC("value method null test 6");

        assertEquals(JavaScriptValue.create(123).toString(context).get().toJavaString(), "123");
        printPositiveTC("value test method test 1");
        assertEquals(JavaScriptValue.create(123).toBoolean(context).get(), true);
        assertEquals(JavaScriptValue.create(0).toBoolean(context).get(), false);
        printPositiveTC("value test method test 2");
        assertEquals(JavaScriptValue.create(123.123).toInteger(context).get(), Double.valueOf(123.0));
        assertEquals(JavaScriptValue.create(123.456).toInt32(context).get(), Integer.valueOf(123));
        printPositiveTC("value test method test 3");
        assertEquals(JavaScriptString.create("123").toNumber(context).get(), Double.valueOf(123));
        assertTrue(JavaScriptString.create("123").toObject(context).get().isObject());
        printPositiveTC("value test method test 4");
        assertFalse(context.lastThrownException().isPresent());
        assertFalse(JavaScriptValue.createUndefined().toObject(context).isPresent());
        assertTrue(context.lastThrownException().isPresent());

        context = null;
        finalizeEngine();
    }

    @Test
    public void symbolValueTest() {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        JavaScriptSymbol symbol = JavaScriptSymbol.create(Optional.empty());
        assertFalse(symbol.description().isPresent());
        assertTrue(symbol.symbolDescriptiveString().toJavaString().equals("Symbol()"));
        printNegativeTC("symbol null test 1");

        symbol = JavaScriptSymbol.create((Optional<JavaScriptString>) null);
        assertFalse(symbol.description().isPresent());
        assertTrue(symbol.symbolDescriptiveString().toJavaString().equals("Symbol()"));
        printNegativeTC("symbol null test 2");

        symbol = JavaScriptSymbol.create(Optional.of(JavaScriptString.create("foobar")));
        assertTrue(symbol.description().isPresent());
        assertTrue(symbol.description().get().toJavaString().equals("foobar"));
        assertFalse(symbol.equalsTo(context, JavaScriptSymbol.create(Optional.of(JavaScriptString.create("foobar")))).get().booleanValue());
        printNegativeTC("symbol null test 4");

        Optional<JavaScriptValue> exception = context.lastThrownException();
        assertFalse(exception.isPresent());
        Optional<JavaScriptString> shouldBeEmpty = symbol.toString(context);
        assertFalse(shouldBeEmpty.isPresent());
        exception = context.lastThrownException();
        assertTrue(exception.isPresent());
        assertEquals(exception.get().toString(context).get().toJavaString(), "TypeError: Cannot convert a Symbol value to a string");
        printNegativeTC("symbol null test 5");

        assertThrows(NullPointerException.class, () -> {
            JavaScriptSymbol.fromGlobalSymbolRegistry(null, JavaScriptString.create("foo"));
        });
        printNegativeTC("symbol null test 6");
        assertThrows(NullPointerException.class, () -> {
            JavaScriptSymbol.fromGlobalSymbolRegistry(vmInstance, null);
        });
        printNegativeTC("symbol null test 7");

        JavaScriptSymbol symbol1 = JavaScriptSymbol.fromGlobalSymbolRegistry(vmInstance, JavaScriptString.create("foo"));
        JavaScriptSymbol symbol2 = JavaScriptSymbol.fromGlobalSymbolRegistry(vmInstance, JavaScriptString.create("foo"));
        assertTrue(symbol1.equalsTo(context, symbol2).get().booleanValue());
        printPositiveTC("symbol test 1");

        context = null;
        finalizeEngine();
    }

    @Test
    public void objectCreateReadWriteTest() {
        Context context = initEngineAndCreateContext();

        assertThrows(NullPointerException.class, () -> {
            JavaScriptObject.create((Context) null);
        });
        printNegativeTC("object null test 1");

        JavaScriptObject obj = JavaScriptObject.create(context);

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                obj.get(null, JavaScriptString.create("asdf"));
            });
            printNegativeTC("object null test 2");
            assertThrows(NullPointerException.class, () -> {
                obj.get(finalContext, null);
            });
            printNegativeTC("object null test 3");
            assertThrows(NullPointerException.class, () -> {
                obj.set(null, JavaScriptString.create("asdf"), JavaScriptString.create("asdf"));
            });
            printNegativeTC("object null test 4");
            assertThrows(NullPointerException.class, () -> {
                obj.set(finalContext, null, JavaScriptString.create("asdf"));
            });
            printNegativeTC("object null test 5");
            assertThrows(NullPointerException.class, () -> {
                obj.set(finalContext, JavaScriptString.create("asdf"), null);
            });
            printNegativeTC("object null test 6");
        }

        assertTrue(obj.set(context, JavaScriptString.create("asdf"), JavaScriptValue.create(123)).get().booleanValue());
        printPositiveTC("object test 1");
        assertTrue(obj.get(context, JavaScriptString.create("asdf")).get().toNumber(context).get().doubleValue() == 123);
        printPositiveTC("object test 2");

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                obj.defineDataProperty(null, JavaScriptString.create("qwer"), JavaScriptValue.create(123), false, false, false);
            });
            printNegativeTC("object null test 6");
            assertThrows(NullPointerException.class, () -> {
                obj.defineDataProperty(finalContext, null, JavaScriptValue.create(123), false, false, false);
            });
            printNegativeTC("object null test 7");
            assertThrows(NullPointerException.class, () -> {
                obj.defineDataProperty(finalContext, null, null, false, false, false);
            });
            printNegativeTC("object null test 8");
        }

        assertTrue(obj.defineDataProperty(context, JavaScriptString.create("qwer"), JavaScriptValue.create(123), false, false, false).get().booleanValue());
        printPositiveTC("object test 3");
        assertFalse(obj.defineDataProperty(context, JavaScriptString.create("qwer"), JavaScriptValue.create(456), false, true, true).get().booleanValue());
        printPositiveTC("object test 4");

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                obj.getOwnProperty(null, JavaScriptString.create("qwer"));
            });
            printNegativeTC("object null test 9");
            assertThrows(NullPointerException.class, () -> {
                obj.getOwnProperty(finalContext, null);
            });
            printNegativeTC("object null test 10");
        }

        assertTrue(obj.getOwnProperty(context, JavaScriptString.create("qwer")).get().toNumber(context).get().doubleValue() == 123);
        printPositiveTC("object test 5");

        context = null;
        finalizeEngine();
    }

    @Test
    public void arrayCreateReadWriteTest() {
        Context context = initEngineAndCreateContext();

        assertThrows(NullPointerException.class, () -> {
            JavaScriptArrayObject.create((Context) null);
        });
        printNegativeTC("array null test 1");

        JavaScriptArrayObject arr = JavaScriptArrayObject.create(context);
        assertTrue(arr.set(context, JavaScriptValue.create(3), JavaScriptValue.create(123)).get().booleanValue());
        assertTrue(arr.get(context, JavaScriptValue.create(3)).get().toNumber(context).get().doubleValue() == 123);
        assertTrue(arr.get(context, JavaScriptString.create("length")).get().toInt32(context).get().intValue() == 4);
        assertTrue(arr.length(context) == 4);
        printPositiveTC("array test 1");

        assertThrows(NullPointerException.class, () -> {
            arr.length(null);
        });
        printNegativeTC("array null test 2");

        JavaScriptArrayObject arr2 = JavaScriptArrayObject.create(context, new JavaScriptValue[]{
            JavaScriptValue.create(123),
            JavaScriptString.create("456")
        }).get();
        assertTrue(arr2.length(context) == 2);
        assertTrue(arr2.get(context, JavaScriptValue.create(0)).get().toNumber(context).get().doubleValue() == 123);
        assertTrue(arr2.get(context, JavaScriptValue.create(1)).get().toNumber(context).get().doubleValue() == 456);
        printPositiveTC("array test 3");

        context = null;
        finalizeEngine();
    }

    @Test
    public void jsonParseStringifyTest() {
        Context context = initEngineAndCreateContext();

        String testString = "[1, 2, 3]";

        {
            final Context finalContext = context;
            String finalTestString = testString;
            assertThrows(NullPointerException.class, () -> {
                finalContext.getGlobalObject().jsonParse(null, JavaScriptString.create(finalTestString));
            });
            printNegativeTC("json null test 1");
            assertThrows(NullPointerException.class, () -> {
                finalContext.getGlobalObject().jsonParse(finalContext, null);
            });
            printNegativeTC("json null test 2");
        }

        JavaScriptValue result = context.getGlobalObject().jsonParse(context, JavaScriptString.create(testString)).get();
        assertTrue(result.isArrayObject());
        assertEquals(result.asScriptArrayObject().get(context, JavaScriptValue.create(0)).get().toNumber(context).get().intValue(), 1);
        assertEquals(result.asScriptArrayObject().get(context, JavaScriptValue.create(1)).get().toNumber(context).get().intValue(), 2);
        assertEquals(result.asScriptArrayObject().get(context, JavaScriptValue.create(2)).get().toNumber(context).get().intValue(), 3);
        assertEquals(result.asScriptArrayObject().length(context), 3);
        printPositiveTC("json test 1");

        testString = "{\"a\": \"asdf\"}";
        result = context.getGlobalObject().jsonParse(context, JavaScriptString.create(testString)).get();
        assertTrue(result.isObject());
        assertEquals(result.asScriptObject().get(context, JavaScriptString.create("a")).get().asScriptString().toJavaString(), "asdf");
        result.asScriptObject().set(context, JavaScriptValue.create(123), JavaScriptValue.create(456));
        printPositiveTC("json test 2");

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                finalContext.getGlobalObject().jsonStringify(null, JavaScriptValue.create(100));
            });
            printNegativeTC("json null test 3");
            assertThrows(NullPointerException.class, () -> {
                finalContext.getGlobalObject().jsonStringify(finalContext, null);
            });
            printNegativeTC("json null test 4");
        }

        assertEquals(context.getGlobalObject().jsonStringify(context, result).get().toJavaString(), "{\"123\":456,\"a\":\"asdf\"}");
        printPositiveTC("json test 3");

        context = null;
        finalizeEngine();
    }

    @Test
    public void testCallableAndCall() {
        Context context = initEngineAndCreateContext();

        JavaScriptValue value = JavaScriptString.create(1123);
        assertFalse(value.isCallable());
        value = JavaScriptString.create("1123");
        assertFalse(value.isCallable());
        printPositiveTC("callable test 1");
        value = JavaScriptSymbol.create(Optional.of(JavaScriptString.create("asdf")));
        assertFalse(value.isCallable());
        value = JavaScriptObject.create(context);
        assertFalse(value.isCallable());
        value = JavaScriptArrayObject.create(context);
        assertFalse(value.isCallable());
        printPositiveTC("callable test 2");

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                JavaScriptString.create("asdf").call(null, JavaScriptValue.createUndefined(), new JavaScriptValue[]{});
            });
            printNegativeTC("callable null test 1");
            assertThrows(NullPointerException.class, () -> {
                JavaScriptString.create("asdf").call(finalContext, null, new JavaScriptValue[]{});
            });
            printNegativeTC("callable null test 2");
            assertThrows(NullPointerException.class, () -> {
                JavaScriptString.create("asdf").call(finalContext, JavaScriptValue.createUndefined(), null);
            });
            printNegativeTC("callable null test 3");
        }

        assertFalse(context.lastThrownException().isPresent());
        value.call(context, JavaScriptValue.createUndefined(), new JavaScriptValue[]{});
        assertTrue(context.lastThrownException().isPresent());
        printPositiveTC("callable test 3");

        value = context.getGlobalObject().asScriptObject().get(context, JavaScriptString.create("Array")).get();
        assertTrue(value.isCallable());

        value = value.call(context, JavaScriptString.createUndefined(), new JavaScriptValue[]{
                JavaScriptValue.create(1), JavaScriptValue.create(2), JavaScriptValue.create(3)
        }).get();

        assertTrue(value.isArrayObject());
        assertEquals(value.asScriptArrayObject().length(context), 3);
        assertEquals(value.asScriptArrayObject().get(context, JavaScriptValue.create(0)).get().asInt32(), 1);
        assertEquals(value.asScriptArrayObject().get(context, JavaScriptValue.create(1)).get().asInt32(), 2);
        assertEquals(value.asScriptArrayObject().get(context, JavaScriptValue.create(2)).get().asInt32(), 3);
        printPositiveTC("callable test 4");

        // receiver test
        JavaScriptGlobalObject global = context.getGlobalObject();
        JavaScriptValue globalFunction = global.get(context, JavaScriptString.create("Function")).get();
        JavaScriptValue newFunction = globalFunction.call(context, JavaScriptValue.createUndefined(),
                new JavaScriptValue[]{JavaScriptString.create("return this")}).get();
        assertTrue(newFunction.isFunctionObject());
        JavaScriptValue ret = newFunction.call(context, JavaScriptValue.createUndefined(), new JavaScriptValue[]{}).get();
        assertTrue(ret.equalsTo(context, global).get().booleanValue());

        ret = newFunction.call(context, JavaScriptString.create("asdf"), new JavaScriptValue[]{}).get();
        assertTrue(ret.isObject());
        assertEquals(ret.toString(context).get().toJavaString(), "asdf");
        printPositiveTC("callable test 5");

        context = null;
        finalizeEngine();
    }

    @Test
    public void testBigInt() {
        Context context = initEngineAndCreateContext();

        JavaScriptBigInt bigInt = JavaScriptBigInt.create(123123);
        assertTrue(bigInt.asScriptBigInt().toNumber() == 123123);
        assertEquals(bigInt.toString(10).toJavaString(), "123123");
        assertEquals(bigInt.toString(3).toJavaString(), "20020220010");
        assertEquals(bigInt.toInt64(), 123123);
        assertTrue(bigInt.isBigInt());
        assertFalse(JavaScriptValue.createNull().isBigInt());
        printPositiveTC("BigInt test 1");

        bigInt = JavaScriptBigInt.create("123123", 10);
        assertEquals(bigInt.toInt64(), 123123);

        bigInt = JavaScriptBigInt.create(JavaScriptString.create("123123"), 10);
        assertEquals(bigInt.toInt64(), 123123);

        assertTrue(bigInt.equalsTo(context, JavaScriptBigInt.create("20020220010", 3)).get().booleanValue());
        printPositiveTC("BigInt test 2");

        bigInt = JavaScriptBigInt.create(Long.MAX_VALUE + "0", 10);
        assertEquals(bigInt.toString(10).toJavaString(), Long.MAX_VALUE + "0");
        assertEquals(bigInt.toInt64(), -10);
        printNegativeTC("BigInt value test 1");

        bigInt = JavaScriptBigInt.create(Long.MAX_VALUE);
        assertEquals(bigInt.toInt64(), Long.MAX_VALUE);
        printNegativeTC("BigInt value test 2");

        bigInt = JavaScriptBigInt.create(Long.MIN_VALUE);
        assertEquals(bigInt.toInt64(), Long.MIN_VALUE);
        printNegativeTC("BigInt value test 3");

        assertEquals(JavaScriptBigInt.create((String) null, 10).toInt64(), 0);
        assertEquals(JavaScriptBigInt.create((JavaScriptString) null, 10).toInt64(), 0);
        printNegativeTC("BigInt value test 4");

        context = null;
        finalizeEngine();
    }

    @Test
    public void testConstruct() {
        Context context = initEngineAndCreateContext();

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                JavaScriptString.create("asdf").construct(null, new JavaScriptValue[]{});
            });
            printNegativeTC("construct value test 1");
            assertThrows(NullPointerException.class, () -> {
                JavaScriptString.create("asdf").construct(finalContext, null);
            });
            printNegativeTC("construct value test 2");
        }

        JavaScriptGlobalObject global = context.getGlobalObject();
        JavaScriptValue globalFunction = global.get(context, JavaScriptString.create("Function")).get();
        JavaScriptValue newFunction = globalFunction.construct(context, new JavaScriptValue[]{
                JavaScriptString.create("a"),
                JavaScriptString.create("return a")
        }).get();

        assertTrue(newFunction.isFunctionObject());
        printPositiveTC("construct test 1");

        JavaScriptValue returnValue = newFunction.call(context, JavaScriptValue.createUndefined(), new JavaScriptValue[]{
                JavaScriptString.create("test")
        }).get();

        assertEquals(returnValue.asScriptString().toJavaString(), "test");
        printPositiveTC("construct test 2");

        context = null;
        finalizeEngine();
    }

    @Test
    public void testJavaCallbackFunction() {
        Context context = initEngineAndCreateContext();

        {
            final Context finalContext = context;
            assertThrows(NullPointerException.class, () -> {
                JavaScriptJavaCallbackFunctionObject.create(null,
                        null,
                        3,
                        false,
                        new JavaScriptJavaCallbackFunctionObject.Callback() {
                            @Override
                            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                                return Optional.empty();
                            }
                        });
            });
            printNegativeTC("callback null test 1");
            assertThrows(NullPointerException.class, () -> {
                JavaScriptJavaCallbackFunctionObject.create(finalContext,
                        null,
                        3,
                        false,
                        null);
            });
            printNegativeTC("callback null test 2");
        }

        JavaScriptJavaCallbackFunctionObject callbackFunctionObject =
                JavaScriptJavaCallbackFunctionObject.create(context,
                        "fnname",
                        3,
                        false,
                        new JavaScriptJavaCallbackFunctionObject.Callback() {
                            @Override
                            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                                return Optional.of(JavaScriptValue.create(arguments.length));
                            }
                        });
        callbackFunctionObject.setExtraData(Optional.of(new Object()));

        context.getGlobalObject().set(context, JavaScriptString.create("asdf"), callbackFunctionObject);

        Optional<JavaScriptValue> ret = Evaluator.evalScript(context, "asdf.name", "test.js", false);
        assertEquals(ret.get().asScriptString().toJavaString(), "fnname");

        ret = Evaluator.evalScript(context, "asdf.length", "test.js", false);
        assertEquals(ret.get().asInt32(), 3);
        printPositiveTC("construct test 1");

        assertFalse(context.exceptionWasThrown());
        Evaluator.evalScript(context, "new asdf()", "test.js", false);
        assertFalse(context.lastThrownException().isPresent());

        ret = Evaluator.evalScript(context, "asdf(1, 2, 3, 4)", "test.js", false);
        assertEquals(ret.get().asInt32(), 4);

        ret = Evaluator.evalScript(context, "asdf(1, 2)", "test.js", false);
        assertEquals(ret.get().asInt32(), 2);
        printPositiveTC("construct test 2");

        callbackFunctionObject =
                JavaScriptJavaCallbackFunctionObject.create(context,
                        "fnname",
                        3,
                        false,
                        new JavaScriptJavaCallbackFunctionObject.Callback() {
                            @Override
                            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                                int sum = 0;
                                for (int i = 0; i < arguments.length; i++) {
                                    sum += arguments[i].asInt32();
                                }
                                return Optional.of(JavaScriptValue.create(sum));
                            }
                        });
        callbackFunctionObject.setExtraData(Optional.of(new Object()));
        context.getGlobalObject().set(context, JavaScriptString.create("asdf"), callbackFunctionObject);

        ret = Evaluator.evalScript(context, "asdf(1, 2, 3, 4)", "test.js", false);
        assertEquals(ret.get().asInt32(), 10);

        ret = Evaluator.evalScript(context, "asdf(1, 2)", "test.js", false);
        assertEquals(ret.get().asInt32(), 3);
        printPositiveTC("construct test 3");

        callbackFunctionObject =
                JavaScriptJavaCallbackFunctionObject.create(context,
                        "fnname",
                        0,
                        false,
                        new JavaScriptJavaCallbackFunctionObject.Callback() {
                            @Override
                            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                                return null;
                            }
                        });
        context.getGlobalObject().set(context, JavaScriptString.create("asdf"), callbackFunctionObject);

        ret = Evaluator.evalScript(context, "asdf()", "test.js", false);
        assertTrue(ret.get().isUndefined());
        printNegativeTC("callback null test 3");

        callbackFunctionObject =
                JavaScriptJavaCallbackFunctionObject.create(context,
                        "fnname",
                        0,
                        false,
                        new JavaScriptJavaCallbackFunctionObject.Callback() {
                            @Override
                            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                                throw new RuntimeException("test");
                            }
                        });
        context.getGlobalObject().set(context, JavaScriptString.create("asdf"), callbackFunctionObject);
        {
            final Context finalContext1 = context;
            assertThrows(RuntimeException.class, () -> {
                finalContext1.getGlobalObject().get(finalContext1, JavaScriptString.create("asdf")).get().call(finalContext1, JavaScriptValue.createUndefined(), new JavaScriptValue[]{});
            });
        }
        printNegativeTC("callback RuntimeException test");

        context = null;
        finalizeEngine();
    }

    @Test
    public void nanInfIssueTest() {
        Context context = initEngineAndCreateContext();

        Optional<JavaScriptValue> ret = Evaluator.evalScript(context, "(this.NaN + '') == 'NaN'", "test.js", false);
        assertTrue(ret.isPresent());
        assertTrue(ret.get().isTrue());
        printNegativeTC("nanInfIssueTest test 1");

        ret = Evaluator.evalScript(context, "(this.Infinity + '') == 'Infinity'", "test.js", false);
        assertTrue(ret.isPresent());
        assertTrue(ret.get().isTrue());
        printNegativeTC("nanInfIssueTest test 2");

        ret = Evaluator.evalScript(context, "(parseFloat(undefined) + '') == 'NaN'", "test.js", false);
        assertTrue(ret.isPresent());
        assertTrue(ret.get().isTrue());
        printNegativeTC("nanInfIssueTest test 3");

        ret = Evaluator.evalScript(context, "(undefined + undefined) ? 'a' : 'b' == 'b'", "test.js", false);
        assertTrue(ret.isPresent());
        assertTrue(ret.get().isTrue());
        printNegativeTC("nanInfIssueTest test 4");

        context = null;
        finalizeEngine();
    }

    @Test
    public void promiseTest()
    {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        Evaluator.evalScript(context, "var myResolve\n" +
                "var myPromise = new Promise((resolve, reject) => {\n" +
                "  myResolve = resolve;\n" +
                "});\n" +
                "myPromise.then( () => { globalThis.thenCalled = true; } )", "test.js", true);

        assertTrue(context.getGlobalObject().get(context, JavaScriptString.create("thenCalled")).get().isUndefined());
        Evaluator.evalScript(context, "myResolve()", "test.js", true);
        assertTrue(context.getGlobalObject().get(context, JavaScriptString.create("thenCalled")).get().asBoolean());
        assertFalse(vmInstance.hasPendingJob());
        printPositiveTC("promiseTest 1");

        context = Context.create(vmInstance);
        Evaluator.evalScript(context, "var myResolve\n" +
                "var myPromise = new Promise((resolve, reject) => {\n" +
                "  myResolve = resolve;\n" +
                "});\n" +
                "myPromise.then( () => { globalThis.thenCalled = true; } )", "test.js", true);
        context.getGlobalObject().get(context, JavaScriptString.create("myResolve")).get().call(context, JavaScriptValue.createUndefined(), new JavaScriptValue[]{});
        assertTrue(vmInstance.hasPendingJob());
        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(context.getGlobalObject().get(context, JavaScriptString.create("thenCalled")).get().asBoolean());
        assertFalse(vmInstance.hasPendingJob());

        printPositiveTC("promiseTest 2");
        JavaScriptPromiseObject po = JavaScriptPromiseObject.create(context);
        assertTrue(po.isPromiseObject());
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Pending);
        assertTrue(po.promiseResult().isUndefined());
        assertFalse(po.isArrayObject());

        printPositiveTC("promiseTest 3");
        class TestCallback extends JavaScriptJavaCallbackFunctionObject.Callback {
            public boolean called = false;
            @Override
            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                JavaScriptObject.create(context);
                called = true;
                assertTrue(arguments[0].asInt32() == 123);
                return Optional.of(JavaScriptString.create("asdf"));
            }
        }
        TestCallback cb = new TestCallback();
        assertFalse(cb.called);
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Pending);
        JavaScriptObject newPromise = po.then(context, JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb)).get().asScriptPromiseObject();
        assertTrue(newPromise.isPromiseObject());
        po.fulfill(context, JavaScriptValue.create(123));
        assertFalse(cb.called);
        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(cb.called);
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.FulFilled);

        printPositiveTC("promiseTest 4");
        po = JavaScriptPromiseObject.create(context);
        System.out.println(po.state().toString());
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Pending);
        cb = new TestCallback();
        assertFalse(cb.called);
        newPromise = po.catchOperation(context, JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb)).get().asScriptPromiseObject();
        assertTrue(newPromise.isPromiseObject());
        po.reject(context, JavaScriptValue.create(123));
        assertFalse(cb.called);
        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(cb.called);
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Rejected);

        {
            JavaScriptPromiseObject finalPo = po;
            Context finalContext = context;
            JavaScriptObject finalCb = JavaScriptJavaCallbackFunctionObject.create(finalContext, "asdf", 1, false, cb);
            printNegativeTC("promiseTest 5");
            assertThrows(NullPointerException.class, () -> {
                finalPo.fulfill(null, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.fulfill(finalContext, null);
            });

            printNegativeTC("promiseTest 6");
            assertThrows(NullPointerException.class, () -> {
                finalPo.reject(null, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.reject(finalContext, null);
            });

            printNegativeTC("promiseTest 7");
            assertThrows(NullPointerException.class, () -> {
                finalPo.then(null, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.then(finalContext, null);
            });

            assertThrows(NullPointerException.class, () -> {
                finalPo.then(null, finalCb, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.then(finalContext, null, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.then(finalContext, finalCb, null);
            });

            printNegativeTC("promiseTest 7");
            assertThrows(NullPointerException.class, () -> {
                finalPo.catchOperation(null, finalCb);
            });
            assertThrows(NullPointerException.class, () -> {
                finalPo.catchOperation(finalContext, null);
            });
        }

        printPositiveTC("promiseTest 8");
        po = JavaScriptPromiseObject.create(context);
        System.out.println(po.state().toString());
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Pending);
        cb = new TestCallback();
        TestCallback cb2 = new TestCallback();
        assertFalse(cb.called);
        assertFalse(cb2.called);
        newPromise = po.then(context, JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb)
                , JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb2)).get().asScriptPromiseObject();
        assertTrue(newPromise.isPromiseObject());
        po.fulfill(context, JavaScriptValue.create(123));
        assertFalse(cb.called);
        assertFalse(cb2.called);
        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(cb.called);
        assertFalse(cb2.called);
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.FulFilled);

        printPositiveTC("promiseTest 9");
        po = JavaScriptPromiseObject.create(context);
        assertFalse(po.hasHandler());
        System.out.println(po.state().toString());
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Pending);
        cb = new TestCallback();
        cb2 = new TestCallback();
        assertFalse(cb.called);
        assertFalse(cb2.called);
        newPromise = po.then(context, JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb)
                , JavaScriptJavaCallbackFunctionObject.create(context, "asdf", 1, false, cb2)).get().asScriptPromiseObject();
        assertTrue(po.hasHandler());
        assertTrue(newPromise.isPromiseObject());
        po.reject(context, JavaScriptValue.create(123));
        assertFalse(cb.called);
        assertFalse(cb2.called);
        vmInstance.executeEveryPendingJobIfExists();
        assertFalse(cb.called);
        assertTrue(cb2.called);
        assertTrue(po.state() == JavaScriptPromiseObject.PromiseState.Rejected);

        printPositiveTC("promiseTest 10");
        Optional<JavaScriptValue> asyncCallResult = Evaluator.evalScript(context, "async function asdf() { return 10; }; asdf().then(function() {});",
                "test.js", false, false);
        assertTrue(asyncCallResult.isPresent());
        assertTrue(vmInstance.hasPendingJob());
        assertTrue(asyncCallResult.get().asScriptPromiseObject().state() == JavaScriptPromiseObject.PromiseState.Pending);
        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(asyncCallResult.get().asScriptPromiseObject().state() == JavaScriptPromiseObject.PromiseState.FulFilled);

        printPositiveTC("promiseTest 11");
        asyncCallResult = Evaluator.evalScript(context, "async function asdf() { return 10; }; asdf().then(function() {});",
                "test.js", false, true);
        assertTrue(asyncCallResult.isPresent());
        assertFalse(vmInstance.hasPendingJob());
        assertTrue(asyncCallResult.get().asScriptPromiseObject().state() == JavaScriptPromiseObject.PromiseState.FulFilled);

        context = null;
        vmInstance = null;
        finalizeEngine();
    }

    @Test
    public void extraDataTest()
    {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        JavaScriptObject obj = JavaScriptObject.create(context);
        assertTrue(obj.extraData() != null);
        assertFalse(obj.extraData().isPresent());
        obj.setExtraData(null);
        assertFalse(obj.extraData().isPresent());
        printNegativeTC("extraDataTest 1");

        Object jObject = new Object();

        obj.setExtraData(Optional.of(jObject));
        assertTrue(obj.extraData().isPresent());
        assertEquals(obj.extraData().get(), jObject);
        obj.setExtraData(Optional.empty());
        assertFalse(obj.extraData().isPresent());
        printPositiveTC("promiseTest 2");

        obj.setExtraData(Optional.of(jObject));
        assertTrue(obj.extraData().isPresent());
        assertEquals(obj.extraData().get(), jObject);
        obj.setExtraData(null);
        assertFalse(obj.extraData().isPresent());
        printNegativeTC("promiseTest 3");

        context = null;
        vmInstance = null;
        finalizeEngine();
    }

    @Test
    public void promiseBuiltinTest()
    {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);
        /*
        const promise1 = Promise.resolve(3);
        const promise2 = 42;
        const promise3 = new Promise((resolve, reject) => {
                resolve("foo")
            });

        Promise.all([promise1, promise2, promise3]).then((values) => {
                    console.log(values);
        });
        // Expected output: Array [3, 42, "foo"]
        */
        ArrayList<Object> thenCalled = new ArrayList<>();
        JavaScriptPromiseObject promise3 = JavaScriptPromiseObject.create(context);
        context.getGlobalObject().promiseAll(
                context,
                JavaScriptArrayObject.create(context, new JavaScriptValue[] {
                        context.getGlobalObject().promiseResolve(context, JavaScriptValue.create(3)).get(),
                        JavaScriptValue.create(42),
                        promise3
                }).get()
        ).get().asScriptPromiseObject().then(context, JavaScriptJavaCallbackFunctionObject.create(context,
                "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        assertTrue(arguments[0].asScriptArrayObject().length(context) == 3);
                        assertTrue(arguments[0].asScriptArrayObject().get(context, JavaScriptValue.create(0)).get().asInt32() == 3);
                        assertTrue(arguments[0].asScriptArrayObject().get(context, JavaScriptValue.create(1)).get().asInt32() == 42);
                        assertTrue(arguments[0].asScriptArrayObject().get(context, JavaScriptValue.create(2)).get().asScriptString().toJavaString().equals("foo"));
                        return Optional.empty();
                    }
                }));

        promise3.fulfill(context, JavaScriptString.create("foo"));

        vmInstance.executeEveryPendingJobIfExists();

        assertTrue(thenCalled.size() == 1);

        /*
        const promise1 = Promise.resolve(3);
        const promise2 = Promise.resolve(0);
        const promises = [promise1, promise2];

                Promise.allSettled(promises).then((results) => results.forEach((result) => console.log(result.status)));

        // Expected output:
        // "fulfilled"
        // "rejected"
        */
        thenCalled.clear();
        context.getGlobalObject().promiseAllSettled(
                context,
                JavaScriptArrayObject.create(context, new JavaScriptValue[] {
                        context.getGlobalObject().promiseResolve(context, JavaScriptValue.create(3)).get(),
                        context.getGlobalObject().promiseReject(context, JavaScriptValue.create(0)).get(),
                }).get()
        ).get().asScriptPromiseObject().then(context, JavaScriptJavaCallbackFunctionObject.create(context,
                "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        JavaScriptArrayObject arr = arguments[0].asScriptArrayObject();
                        assertTrue(arr.length(context) == 2);
                        assertEquals(arr.get(context, JavaScriptValue.create(0)).get().asScriptObject().get(context, JavaScriptString.create("status")).get().asScriptString().toJavaString(), "fulfilled");
                        assertEquals(arr.get(context, JavaScriptValue.create(1)).get().asScriptObject().get(context, JavaScriptString.create("status")).get().asScriptString().toJavaString(), "rejected");
                        return Optional.empty();
                    }
                }));

        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(thenCalled.size() == 1);

        /*
            const promise1 = Promise.reject(0);
            const promise2 = new Promise(function(resolve) {resolve("foo")});
            const promise3 = new Promise(function(resolve) {

            });

            const promises = [promise1, promise2, promise3];
            Promise.any(promises).then((value) => console.log(value));
         */
        thenCalled.clear();
        context.getGlobalObject().promiseAny(
                context,
                JavaScriptArrayObject.create(context, new JavaScriptValue[] {
                        context.getGlobalObject().promiseReject(context, JavaScriptValue.create(0)).get(),
                        context.getGlobalObject().promiseResolve(context, JavaScriptString.create("foo")).get(),
                        JavaScriptPromiseObject.create(context),
                }).get()
        ).get().asScriptPromiseObject().then(context, JavaScriptJavaCallbackFunctionObject.create(context,
                "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        assertEquals("foo", arguments[0].asScriptString().toJavaString());
                        return Optional.empty();
                    }
                }));

        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(thenCalled.size() == 1);

        thenCalled.clear();
        JavaScriptPromiseObject promise1 = JavaScriptPromiseObject.create(context);
        JavaScriptPromiseObject promise2 = JavaScriptPromiseObject.create(context);
        context.getGlobalObject().promiseRace(
                context,
                JavaScriptArrayObject.create(context, new JavaScriptValue[] {
                        promise1, promise2
                }).get()
        ).get().asScriptPromiseObject().then(context, JavaScriptJavaCallbackFunctionObject.create(context,
                "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        assertEquals("foo", arguments[0].asScriptString().toJavaString());
                        return Optional.empty();
                    }
                }));

        promise2.fulfill(context, JavaScriptString.create("foo"));
        promise1.fulfill(context, JavaScriptString.create("bar"));

        vmInstance.executeEveryPendingJobIfExists();
        assertTrue(thenCalled.size() == 1);

        printPositiveTC("promiseBuiltinTest 1");
        context = null;
        vmInstance = null;
        finalizeEngine();
    }

    @Test
    public void multiThreadExecutorTest() throws InterruptedException {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        MultiThreadExecutor executor = new MultiThreadExecutor(vmInstance);
        int count = 100;
        JavaScriptArrayObject arr = JavaScriptArrayObject.create(context);
        for (int i = 0; i < count; i ++) {
            final Integer idx = i;
            MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
                @Override
                public MultiThreadExecutor.ResultBuilderContext run() {
                    // System.out.println("worker executed on worker thread!");
                    try {
                        Thread.sleep((long) (Math.abs(Math.random()) * 1000));
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                    return new MultiThreadExecutor.ResultBuilderContext(true, new String("asdf" + idx));
                }
            }, new MultiThreadExecutor.ResultBuilder() {
                @Override
                public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                    // System.out.println("worker result built on JavaScript thread!");
                    return JavaScriptString.create((String)builderContext.data());
                }
            });
            arr.set(context, JavaScriptValue.create(i), instance.promise());
        }

        // add another dummy task for test
        for (int i = 0; i < count; i ++) {
            final Integer idx = i;
            MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
                @Override
                public MultiThreadExecutor.ResultBuilderContext run() {
                    // System.out.println("worker executed on worker thread!");
                    try {
                        Thread.sleep((long) (Math.abs(Math.random()) * 1000));
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                    return new MultiThreadExecutor.ResultBuilderContext(true, new String("asdf" + idx));
                }
            }, new MultiThreadExecutor.ResultBuilder() {
                @Override
                public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                    // System.out.println("worker result built on JavaScript thread!");
                    return JavaScriptString.create((String)builderContext.data());
                }
            });
        }

        ArrayList<Object> thenCalled = new ArrayList<>();
        context.getGlobalObject().promiseAll(context, arr).get().asScriptPromiseObject().then(context,
                JavaScriptJavaCallbackFunctionObject.create(context, "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        assertTrue(arguments[0].asScriptArrayObject().length(context) == count);
                        for (int i = 0; i < count; i ++) {
                            JavaScriptValue v = arguments[0].asScriptArrayObject().get(context, JavaScriptValue.create(i)).get();
                            assertEquals(v.asScriptString().toJavaString(), "asdf" + i);
                        }
                        return Optional.empty();
                    }
                }));

        while (executor.hasPendingEventOrWorker()) {
            executor.pumpEventsFromThreadIfNeeds();
        }

        assertTrue(thenCalled.size() == 1);
        printPositiveTC("MultiThreadExecutor 1");

        arr = JavaScriptArrayObject.create(context);
        for (int i = 0; i < count; i ++) {
            final Integer idx = i;
            MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
                @Override
                public MultiThreadExecutor.ResultBuilderContext run() {
                    // System.out.println("worker executed on worker thread!");
                    try {
                        Thread.sleep((long) (Math.abs(Math.random()) * 1000));
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                    return new MultiThreadExecutor.ResultBuilderContext(idx % 2 == 0, new String("asdf" + idx));
                }
            }, new MultiThreadExecutor.ResultBuilder() {
                @Override
                public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                    // System.out.println("worker result built on JavaScript thread!");
                    return JavaScriptString.create((String)builderContext.data());
                }
            });
            arr.set(context, JavaScriptValue.create(i), instance.promise());
        }

        thenCalled.clear();
        context.getGlobalObject().promiseAllSettled(context, arr).get().asScriptPromiseObject().then(context,
                JavaScriptJavaCallbackFunctionObject.create(context, "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
                    @Override
                    public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                        thenCalled.add(new Object());
                        assertTrue(arguments[0].asScriptArrayObject().length(context) == count);
                        for (int i = 0; i < count; i ++) {
                            JavaScriptValue v = arguments[0].asScriptArrayObject().get(context, JavaScriptValue.create(i)).get();
                            String s = (i % 2 == 0) ? "fulfilled" : "rejected";
                            assertEquals(s, v.asScriptObject().get(context, JavaScriptString.create("status")).get().asScriptString().toJavaString());
                            System.out.println(context.getGlobalObject().jsonStringify(context, v).get().toJavaString());

                            if ((i % 2 == 0)) {
                                v = v.asScriptObject().get(context, JavaScriptString.create("value")).get();
                            } else {
                                v = v.asScriptObject().get(context, JavaScriptString.create("reason")).get();
                            }
                            assertEquals(v.asScriptString().toJavaString(), "asdf" + i);
                        }
                        return Optional.empty();
                    }
                }));

        while (executor.hasPendingEventOrWorker()) {
            executor.pumpEventsFromThreadIfNeeds();
        }


        Context finalContext = context;
        MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
            @Override
            public MultiThreadExecutor.ResultBuilderContext run() {
                // System.out.println("worker executed on worker thread!");
                assertThrows(RuntimeException.class, () -> {
                    executor.pumpEventsFromThreadIfNeeds();
                });
                printNegativeTC("MultiThreadExecutor 2");
                return new MultiThreadExecutor.ResultBuilderContext(true, new String("asdf"));
            }
        }, new MultiThreadExecutor.ResultBuilder() {
            @Override
            public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                // System.out.println("worker result built on JavaScript thread!");
                return JavaScriptString.create((String)builderContext.data());
            }
        });


        Thread r = new Thread(new Runnable() {
            @Override
            public void run() {
                assertThrows(RuntimeException.class, () -> {
                    executor.startWorker(finalContext, new MultiThreadExecutor.Executor() {
                        @Override
                        public MultiThreadExecutor.ResultBuilderContext run() {
                            return null;
                        }
                    }, new MultiThreadExecutor.ResultBuilder() {
                        @Override
                        public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                            return null;
                        }
                    });
                });
                printNegativeTC("MultiThreadExecutor 3");
            }
        });

        r.start();
        r.join();

        context = null;
        vmInstance = null;
        finalizeEngine();
    }

    public void multiThreadExecutorExample() {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        context.getGlobalObject().set(context, JavaScriptString.create("print"), JavaScriptJavaCallbackFunctionObject.create(context, "", 1, false, new JavaScriptJavaCallbackFunctionObject.Callback() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue[] arguments) {
                System.out.println(arguments[0].toString(context).get().toJavaString());
                return Optional.empty();
            }
        }));

        MultiThreadExecutor executor = new MultiThreadExecutor(vmInstance);
        Bridge.register(context, "HTTP", "get", new Bridge.Adapter() {
            @Override
            public Optional<JavaScriptValue> callback(Context context, Optional<JavaScriptValue> data) {
                String url = "";
                if (data.isPresent()) {
                    Optional<JavaScriptString> mayBeString = data.get().toString(context);
                    if (mayBeString.isPresent()) {
                        url = mayBeString.get().toJavaString();
                    }
                }
                final String finalURL = url;
                MultiThreadExecutor.ExecutorInstance instance = executor.startWorker(context, new MultiThreadExecutor.Executor() {
                    @Override
                    public MultiThreadExecutor.ResultBuilderContext run() {
                        try {
                            HttpURLConnection connection = (HttpURLConnection)new URL(finalURL).openConnection();
                            connection.setRequestMethod("GET");
                            int responseCode = connection.getResponseCode();
                            if (responseCode == HttpURLConnection.HTTP_OK) { // success
                                BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                                String inputLine;
                                StringBuffer response = new StringBuffer();

                                while ((inputLine = in.readLine()) != null) {
                                    response.append(inputLine);
                                }
                                in.close();

                                return new MultiThreadExecutor.ResultBuilderContext(true, response.toString());
                            } else {
                                return new MultiThreadExecutor.ResultBuilderContext(false, "error HTTP return code:" + responseCode);
                            }
                        } catch (IOException e) {
                            return new MultiThreadExecutor.ResultBuilderContext(false, e.toString());
                        }
                    }
                }, new MultiThreadExecutor.ResultBuilder() {
                    @Override
                    public JavaScriptValue build(Context scriptContext, MultiThreadExecutor.ResultBuilderContext builderContext) {
                        return JavaScriptString.create((String)builderContext.data());
                    }
                });
                return Optional.of(instance.promise());
            }
        });

        Evaluator.evalScript(context, "" +
                "let promise1 = HTTP.get('https://httpbin.org/get');" +
                "let promise2 = HTTP.get('http://google.com');" +
                "Promise.allSettled([promise1, promise2]).then(function(v) {" +
                "print(JSON.stringify(v));" +
                "print('http all end!');" +
                "});", "", false);

        while (executor.hasPendingEventOrWorker()) {
            executor.pumpEventsFromThreadIfNeeds();
            try {
                Thread.sleep(25);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        context = null;
        vmInstance = null;
        finalizeEngine();
    }

    @Test
    public void errorObjectTest()
    {
        Globals.initializeGlobals();
        VMInstance vmInstance = VMInstance.create(Optional.of("en-US"), Optional.of("Asia/Seoul"));
        Context context = Context.create(vmInstance);

        JavaScriptErrorObject e = JavaScriptErrorObject.create(context, JavaScriptErrorObject.ErrorKind.None, "test");
        assertTrue(e.isErrorObject());
        assertTrue(e.toString(context).get().toJavaString().equals("Error: test"));

        JavaScriptErrorObject.ErrorKind[] kinds = JavaScriptErrorObject.ErrorKind.values();
        for (int i = 1 /* skip None */; i < kinds.length; i ++) {
            e = JavaScriptErrorObject.create(context, kinds[i], "test" + i);
            assertTrue(e.isErrorObject());
            assertTrue(e.toString(context).get().toJavaString().equals(kinds[i].name() + ": test" + i));
        }

        context = null;
        vmInstance = null;
        finalizeEngine();
    }
}