package com.samsung.lwe.escargot;

public class JavaScriptArrayObject extends JavaScriptObject {
    static public native JavaScriptArrayObject create(Context context);
    public native long length(Context context);
}
