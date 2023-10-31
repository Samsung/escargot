package com.samsung.lwe.escargot;

public class JavaScriptErrorObject extends JavaScriptObject {
    protected JavaScriptErrorObject(long nativePointer)
    {
        super(nativePointer);
    }

    enum ErrorKind {
        None,
        ReferenceError,
        TypeError,
        SyntaxError,
        RangeError,
        URIError,
        EvalError,
        AggregateError
    }
    static public native JavaScriptErrorObject create(Context context, ErrorKind kind, String message);
}
