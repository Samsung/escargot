package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptErrorObject extends JavaScriptObject {
    protected JavaScriptErrorObject(long nativePointer)
    {
        super(nativePointer);
    }

    public enum ErrorKind {
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
    public native Optional<JavaScriptString> stack(Context context);
}
