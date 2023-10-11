package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptPromiseObject extends JavaScriptObject {
    protected JavaScriptPromiseObject(long nativePointer)
    {
        super(nativePointer);
    }
    static public native JavaScriptPromiseObject create(Context context);

    enum PromiseState {
        Pending,
        FulFilled,
        Rejected
    };
    public native PromiseState state();
    public native JavaScriptValue promiseResult();

    /**
     *
     * @param context
     * @param handler
     * @return
     */
    public native Optional<JavaScriptObject> then(Context context, JavaScriptValue handler);

    /**
     *
     * @param context
     * @param onFulfilled
     * @param onRejected
     * @return
     */
    public native Optional<JavaScriptObject> then(Context context, JavaScriptValue onFulfilled, JavaScriptValue onRejected);

    /**
     *
     * @param context
     * @param handler
     * @return
     */
    public native Optional<JavaScriptObject> catchOperation(Context context, JavaScriptValue handler);

    /**
     *
     * @param context
     * @param value
     */
    public native void fulfill(Context context, JavaScriptValue value);

    /**
     *
     * @param context
     * @param reason
     */
    public native void reject(Context context, JavaScriptValue reason);

    public native boolean hasHandler();
}
