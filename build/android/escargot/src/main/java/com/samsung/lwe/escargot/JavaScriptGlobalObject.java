package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptGlobalObject extends JavaScriptObject {
    protected JavaScriptGlobalObject(long nativePointer)
    {
        super(nativePointer);
    }
    /**
     *
     * @param context
     * @param input
     * @return
     */
    public native Optional<JavaScriptString> jsonStringify(Context context, JavaScriptValue input);

    /**
     *
     * @param context
     * @param input
     * @return
     */
    public native Optional<JavaScriptValue> jsonParse(Context context, JavaScriptValue input);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseAll(Context context, JavaScriptValue iterable);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseAllSettled(Context context, JavaScriptValue iterable);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseAny(Context context, JavaScriptValue iterable);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseRace(Context context, JavaScriptValue iterable);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseReject(Context context, JavaScriptValue iterable);

    /**
     *
     * @param context
     * @param iterable
     * @return
     */
    public native Optional<JavaScriptValue> promiseResolve(Context context, JavaScriptValue iterable);
}
