package com.samsung.lwe.escargot;

import java.util.Optional;

final public class Evaluator {
    static { Escargot.init(); }
    private Evaluator() {
    }

    /**
     * @param context
     * @param source
     * @param sourceFileName
     * @param shouldPrintScriptResult
     * @return return result if eval was successful
     */
    static native public Optional<JavaScriptValue> evalScript(Context context, String source, String sourceFileName, boolean shouldPrintScriptResult);
}
