package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptValue extends NativePointerHolder {
    protected JavaScriptValue(long nativePointer)
    {
        super(nativePointer);
    }

    native static public JavaScriptValue createUndefined();
    native static public JavaScriptValue createNull();
    native static public JavaScriptValue create(boolean value);
    native static public JavaScriptValue create(int value);
    native static public JavaScriptValue create(double value);
    native static public JavaScriptString create(String value);
    native static public JavaScriptSymbol create(Optional<JavaScriptString> value);

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
    native public boolean isObject();
    native public boolean isArrayObject();

    // as{ .. } methods don't check type is correct
    // if you want to use these as{ .. } methods
    // you must check type before use!
    native public boolean asBoolean();
    native public int asInt32();
    native public double asNumber();
    native public JavaScriptString asScriptString();
    native public JavaScriptSymbol asScriptSymbol();
    native public JavaScriptObject asScriptObject();
    native public JavaScriptArrayObject asScriptArrayObject();

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
}
