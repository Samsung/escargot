package com.samsung.lwe.escargot.internal;

import com.samsung.lwe.escargot.JavaScriptValue;

public class JavaScriptRuntimeException extends RuntimeException {
    public JavaScriptRuntimeException(JavaScriptValue exception)
    {
        this.m_exception = exception;
    }

    public JavaScriptValue exception() {
        return m_exception;
    }

    private JavaScriptValue m_exception;
}
