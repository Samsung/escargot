package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptArrayObject extends JavaScriptObject {
    protected JavaScriptArrayObject(long nativePointer)
    {
        super(nativePointer);
    }
    static public native JavaScriptArrayObject create(Context context);

    /**
     *
     * @param context
     * @param from
     * @return
     */
    static public Optional<JavaScriptArrayObject> create(Context context, JavaScriptValue[] from)
    {
        JavaScriptArrayObject ret = JavaScriptArrayObject.create(context);
        ret.setLength(context, from.length);
        for (int i = 0; i < from.length; i ++) {
            Optional<Boolean> b = ret.set(context, JavaScriptValue.create(i), from[i]);
            if (!b.isPresent() || !b.get())
                return Optional.empty();
        }
        return Optional.of(ret);
    }
    public native long length(Context context);
    public native void setLength(Context context, long newLength);
}
