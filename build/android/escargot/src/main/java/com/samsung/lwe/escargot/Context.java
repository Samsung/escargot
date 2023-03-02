package com.samsung.lwe.escargot;

import java.util.Optional;

public class Context extends NativePointerHolder {
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

    native protected void releaseNativePointer();
    @Override
    public void destroy() {
        releaseNativePointer();
    }

    protected Optional<JavaScriptValue> m_lastThrownException = Optional.empty();
}
