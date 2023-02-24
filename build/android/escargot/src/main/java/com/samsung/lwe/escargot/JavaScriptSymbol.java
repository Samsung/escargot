package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptSymbol extends JavaScriptValue {
    native public Optional<JavaScriptString> description();
    native public JavaScriptString symbolDescriptiveString();

    native public static JavaScriptSymbol fromGlobalSymbolRegistry(VMInstance vm, JavaScriptString stringKey);
}
