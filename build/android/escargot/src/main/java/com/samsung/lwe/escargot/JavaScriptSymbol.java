package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptSymbol extends JavaScriptValue {
    protected JavaScriptSymbol(long nativePointer)
    {
        super(nativePointer, true);
    }

    native static public JavaScriptSymbol create(Optional<JavaScriptString> value);
    native public JavaScriptString descriptionString();
    native public JavaScriptValue descriptionValue();
    native public JavaScriptString symbolDescriptiveString();

    /**
     *
     * @param vm
     * @param stringKey
     * @return
     */
    native public static JavaScriptSymbol fromGlobalSymbolRegistry(VMInstance vm, JavaScriptString stringKey);
}
