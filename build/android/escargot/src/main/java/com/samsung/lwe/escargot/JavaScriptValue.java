package com.samsung.lwe.escargot;

public class JavaScriptValue extends NativePointerHolder {
    native static public JavaScriptValue createUndefined();
    native static public JavaScriptValue createNull();
    native static public JavaScriptValue create(boolean value);
    native static public JavaScriptValue create(int value);
    native static public JavaScriptValue create(double value);
    native static public JavaScriptString create(String value);

    native public boolean isUndefined();
    native public boolean isNull();
    native public boolean isUndefinedOrNull();
    native public boolean isBoolean();
    native public boolean isTrue();
    native public boolean isFalse();
    native public boolean isNumber();
    native public boolean isInt32();
    native public boolean isString();

    // as{ .. } method are don't check type is correct
    // if you want to use these as{ .. } methods
    // you must check type before use!
    native public boolean asBoolean();
    native public int asInt32();
    native public double asNumber();
    native public JavaScriptString asScriptString();

    native protected void releaseNativePointer();
    @Override
    public void destroy() {
        releaseNativePointer();
    }
}
