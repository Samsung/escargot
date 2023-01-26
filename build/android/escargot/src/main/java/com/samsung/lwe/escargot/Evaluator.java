package com.samsung.lwe.escargot;

public class Evaluator {
    static native public boolean evalScript(Context context, String source, String sourceFileName, boolean shouldPrintScriptResult);
}
