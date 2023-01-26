package com.samsung.lwe.escargot;

import java.util.Optional;

public class Evaluator {
    // return result as string if eval was successful
    static native public Optional<String> evalScript(Context context, String source, String sourceFileName, boolean shouldPrintScriptResult);
}
