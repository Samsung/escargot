package com.samsung.lwe.escargot;

import java.util.Optional;

public class Context extends NativePointerHolder {
    protected Context(long nativePointer)
    {
        super(nativePointer, true);
    }
    public native static Context create(VMInstance vmInstance);
    public boolean exceptionWasThrown()
    {
        return m_lastThrownException.isPresent();
    }
    public Optional<JavaScriptValue> lastThrownException()
    {
        Optional<JavaScriptValue> lastThrownException = m_lastThrownException;
        m_lastThrownException = Optional.empty();
        return lastThrownException;
    }
    public native JavaScriptGlobalObject getGlobalObject();

    protected Optional<JavaScriptValue> m_lastThrownException = Optional.empty();
}
