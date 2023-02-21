package com.samsung.lwe.escargot;

public class Value extends NativePointerHolder {
    native static public Value createUndefined();
    native static public Value createNull();
    native static public Value create(boolean value);
    native static public Value create(int value);
    native static public Value create(double value);

    native public boolean isUndefined();
    native public boolean isNull();
    native public boolean isBoolean();
    native public boolean isTrue();
    native public boolean isFalse();
    native public boolean isNumber();
    native public boolean isInt32();

    // as{ .. } method are don't type is correct
    // if you want to use these as{ .. } methods
    // you must check type before use!
    native public boolean asBoolean();
    native public int asInt32();
    native public double asNumber();

    native protected void releaseNativePointer();
    @Override
    public void destroy() {
        releaseNativePointer();
    }
}
