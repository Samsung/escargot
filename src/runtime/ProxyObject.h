/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */


#ifndef __EscargotProxyObject__
#define __EscargotProxyObject__

#if ESCARGOT_ENABLE_PROXY_REFLECT

#include "runtime/Object.h"
#include "runtime/FunctionObject.h"

namespace Escargot {

class Proxy;

class RevokeFunctionObject : public FunctionObject {
public:
    RevokeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ProxyObject* proxy)
        : FunctionObject(state, info)
        , m_proxy(proxy)
    {
    }

    ProxyObject* revocableProxy()
    {
        return m_proxy;
    }

    void setRevocableProxyToNull()
    {
        m_proxy = nullptr;
    }

private:
    ProxyObject* m_proxy;
};

class ProxyObject : public Object {
public:
    ProxyObject(ExecutionState& state);

    virtual bool isProxyObject() const
    {
        return true;
    }

    virtual bool isOrdinary() const
    {
        return false;
    }

    virtual bool isCallable() const
    {
        return m_isCallable;
    }

    bool isConstructible() const
    {
        return m_isConstructible;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Proxy";
    }

    static Value createProxy(ExecutionState& state, const Value& target, const Value& handler);

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    virtual bool preventExtensions(ExecutionState& state) override;

    virtual bool hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName) override;

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& propertyName) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver) override;

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual bool setPrototype(ExecutionState& state, const Value& value) override;

    virtual Value getPrototype(ExecutionState&) override;
    virtual Object* getPrototypeObject(ExecutionState&) override;

    virtual bool isExtensible(ExecutionState&) override;

    Value call(ExecutionState& state, const Value& callee, const Value& receiver, const size_t argc, Value* argv);

    Object* construct(ExecutionState& state, const size_t argc, Value* argv);

    void setTarget(Object* target)
    {
        m_target = target;
    }

    Object* target()
    {
        return m_target;
    }

    void setHandler(Object* handler)
    {
        m_handler = handler;
    }

    Object* handler()
    {
        return m_handler;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    bool m_isCallable;
    bool m_isConstructible;

    Object* m_target;
    Object* m_handler;
};
}

#endif // ESCARGOT_ENABLE_PROXY_REFLECT
#endif // __EscargotProxyObject__
