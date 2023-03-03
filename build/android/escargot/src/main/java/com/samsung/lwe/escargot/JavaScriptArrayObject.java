package com.samsung.lwe.escargot;

public class JavaScriptArrayObject extends JavaScriptObject {
    protected JavaScriptArrayObject(long nativePointer)
    {
        super(nativePointer);
    }
    static public native JavaScriptArrayObject create(Context context);
    public native long length(Context context);
}
