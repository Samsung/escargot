package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptJavaCallbackFunctionObject extends JavaScriptFunctionObject {
    protected JavaScriptJavaCallbackFunctionObject(long nativePointer)
    {
        super(nativePointer);
    }

    public abstract static class Callback {
        /**
         *
         * @param context
         * @param receiverValue
         * @param arguments
         * @return
         */
        public abstract Optional<JavaScriptValue> callback(Context context, JavaScriptValue receiverValue, JavaScriptValue arguments[]);
    }

    /**
     *
     * @param context
     * @param functionName
     * @param argumentCount
     * @param isConstructor
     * @param callback
     * @return
     */
    static public native JavaScriptJavaCallbackFunctionObject create(Context context, String functionName, int argumentCount, boolean isConstructor, Callback callback);
}
