package com.samsung.lwe.escargot;

import java.util.Optional;

final public class Bridge {
    static { Escargot.init(); }
    private Bridge() {
    }

    public abstract static class Adapter {
        /**
         * @param data the data parameter contains value when call this function from JavaScript
         * @return if want to return data to JavaScript callback, you can return value from this callback.
         */
        public abstract Optional<JavaScriptValue> callback(Optional<JavaScriptValue> data);
    }

    /**
     * if success, you can access `GlobalObject`.`objectName`.`propertyName`(...) in JavaScript
     *
     * @param context
     * @param objectName
     * @param propertyName
     * @param adapter
     * @return returns true if success
     */
    static public native boolean register(Context context, String objectName, String propertyName, Adapter adapter);
}
