package com.samsung.lwe.escargot;

public class JavaScriptFunctionObject extends JavaScriptObject {
    protected JavaScriptFunctionObject(long nativePointer)
    {
        super(nativePointer);
    }
    public native Context context();
}
