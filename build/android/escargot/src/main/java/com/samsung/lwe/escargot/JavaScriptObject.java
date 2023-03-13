package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptObject extends JavaScriptValue {
    protected JavaScriptObject(long nativePointer)
    {
        super(nativePointer, true);
    }
    static public native JavaScriptObject create(Context context);
    /**
     * return thisObject[propertyName];
     * @param context
     * @param propertyName
     * @return JavaScriptValue or Optional.empty if there was a exception while executing get function
     */
    public native Optional<JavaScriptValue> get(Context context, JavaScriptValue propertyName);

    /**
     * thisObject[propertyName] = value;
     * @param context
     * @param propertyName
     * @param value
     * @return Boolean or Optional.empty if there was a exception while executing set function
     */
    public native Optional<Boolean> set(Context context, JavaScriptValue propertyName, JavaScriptValue value);

    /**
     * Object.definePropety(
     * thisObject,
     * propertyName,
     * {
     *  value: value
     *  writable: isWritable
     *  enumerable: isEnumerable
     *  configurable:  isConfigurable
     * })
     * @param context
     * @param propertyName
     * @param value
     * @param isWritable
     * @param isEnumerable
     * @param isConfigurable
     * @return Boolean or Optional.empty if there was a exception while executing set function
     */
    public native Optional<Boolean> defineDataProperty(Context context, JavaScriptValue propertyName, JavaScriptValue value,
                                                       boolean isWritable, boolean isEnumerable, boolean isConfigurable);

    /**
     * return thisObject[propertyName]; // but only search thisObject. this function does not get value on prototype chain
     * there is no simple equivalent js function or source
     * @param context
     * @param propertyName
     * @return JavaScriptValue or Optional.empty if there was a exception while executing getOwnProperty function
     */
    public native Optional<JavaScriptValue> getOwnProperty(Context context, JavaScriptValue propertyName);
}
