package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptBigInt extends JavaScriptValue {
    protected JavaScriptBigInt(long nativePointer)
    {
        super(nativePointer, true);
    }

    static public native JavaScriptBigInt create(int num);
    static public native JavaScriptBigInt create(long num);

    /**
     *
     * @param numString
     * @param radix
     * @return
     */
    static public native JavaScriptBigInt create(String numString, int radix);

    /**
     *
     * @param numString
     * @param radix
     * @return
     */
    static public native JavaScriptBigInt create(JavaScriptString numString, int radix);

    /**
     *
     * @param radix
     * @return
     */
    public native JavaScriptString toString(int radix);

    public native double toNumber();
    public native long toInt64();
}
