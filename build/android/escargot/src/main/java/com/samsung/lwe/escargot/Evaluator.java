package com.samsung.lwe.escargot;

import java.util.Optional;

final public class Evaluator {
    static { Escargot.init(); }
    private Evaluator() {
    }

    /**
     *
     * @param context
     * @param source
     * @param sourceFileName
     * @return return result if eval was successful
     */
    static public Optional<JavaScriptValue> evalScript(Context context, String source, String sourceFileName)
    {
        return evalScript(context, source, sourceFileName, false);
    }


    /**
     * @param context
     * @param source
     * @param sourceFileName
     * @param shouldPrintScriptResult
     * @return return result if eval was successful
     */
    static public Optional<JavaScriptValue> evalScript(Context context, String source, String sourceFileName,
                                                              boolean shouldPrintScriptResult)
    {
        return evalScript(context, source, sourceFileName, shouldPrintScriptResult, true);
    }

    /**
     * @param context
     * @param source
     * @param sourceFileName
     * @param shouldPrintScriptResult
     * @param shouldExecutePendingJobsAtEnd
     * @return return result if eval was successful
     */
    static native public Optional<JavaScriptValue> evalScript(Context context, String source, String sourceFileName,
                                                              boolean shouldPrintScriptResult, boolean shouldExecutePendingJobsAtEnd);
}
