package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptValue extends NativePointerHolder {
    protected JavaScriptValue(long nativePointer)
    {
        super(nativePointer, true);
    }

    protected JavaScriptValue(long nativePointer, boolean isHeapValue)
    {
        super(nativePointer, isHeapValue);
    }

    native static public JavaScriptValue createUndefined();
    native static public JavaScriptValue createNull();
    native static public JavaScriptValue create(boolean value);
    native static public JavaScriptValue create(int value);
    native static public JavaScriptValue create(double value);

    native public boolean isUndefined();
    native public boolean isNull();
    native public boolean isUndefinedOrNull();
    native public boolean isBoolean();
    native public boolean isTrue();
    native public boolean isFalse();
    native public boolean isNumber();
    native public boolean isInt32();
    native public boolean isString();
    native public boolean isSymbol();
    native public boolean isBigInt();
    native public boolean isCallable();
    native public boolean isObject();
    native public boolean isArrayObject();
    native public boolean isFunctionObject();
    native public boolean isPromiseObject();
    native public boolean isErrorObject();

    native public boolean asBoolean();
    native public int asInt32();
    native public double asNumber();
    native public JavaScriptString asScriptString();
    native public JavaScriptSymbol asScriptSymbol();
    native public JavaScriptBigInt asScriptBigInt();
    native public JavaScriptObject asScriptObject();
    native public JavaScriptArrayObject asScriptArrayObject();
    native public JavaScriptFunctionObject asScriptFunctionObject();
    native public JavaScriptPromiseObject asScriptPromiseObject();
    native public JavaScriptErrorObject asScriptErrorObject();

    native public Optional<JavaScriptString> toString(Context context);
    native public Optional<Boolean> toBoolean(Context context);
    native public Optional<Double> toNumber(Context context);
    native public Optional<Double> toInteger(Context context);
    native public Optional<Integer> toInt32(Context context);
    native public Optional<JavaScriptObject> toObject(Context context);

    /**
     * this == other
     * @param context
     * @param other
     * @return
     */
    native public Optional<Boolean> abstractEqualsTo(Context context, JavaScriptValue other);
    /**
     * this === other
     * @param context
     * @param other
     * @return
     */
    native public Optional<Boolean> equalsTo(Context context, JavaScriptValue other);

    /**
     *
     * @param context
     * @param other
     * @return
     */
    native public Optional<Boolean> instanceOf(Context context, JavaScriptValue other);

    /**
     *
     * @param context
     * @param receiver
     * @param argv
     * @return
     */
    native public Optional<JavaScriptValue> call(Context context, JavaScriptValue receiver, JavaScriptValue[] argv);

    /**
     * same with new expression in js
     * @param context
     * @param argv
     * @return
     */
    native public Optional<JavaScriptValue> construct(Context context, JavaScriptValue[] argv);
}
