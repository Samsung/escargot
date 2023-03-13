package com.samsung.lwe.escargot;

public class JavaScriptString extends JavaScriptValue {
    protected JavaScriptString(long nativePointer)
    {
        super(nativePointer, true);
    }
    native public String toJavaString();
}
