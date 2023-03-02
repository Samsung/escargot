package com.samsung.lwe.escargot;

import java.util.Optional;

public class JavaScriptGlobalObject extends JavaScriptObject {
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
}
