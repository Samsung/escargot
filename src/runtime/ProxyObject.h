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

#include "runtime/Object.h"
#include "runtime/FunctionObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

class Proxy;

class RevokeFunctionObject : public NativeFunctionObject {
public:
    RevokeFunctionObject(ExecutionState& state, NativeFunctionInfo info, ProxyObject* proxy)
        : NativeFunctionObject(state, info)
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

    virtual bool isProxyObject() const override
    {
        return true;
    }

    virtual bool isOrdinary() const override
    {
        return false;
    }

    virtual bool isCallable() const override
    {
        return m_isCallable;
    }

    virtual bool isConstructor() const override
    {
        return m_isConstructible;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Proxy";
    }

    static ProxyObject* createProxy(ExecutionState& state, const Value& target, const Value& handler);

    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    virtual bool preventExtensions(ExecutionState& state) override;

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName) override;

    virtual ValueVector ownPropertyKeys(ExecutionState& state) override;

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& propertyName) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& propertyName, const Value& v, const Value& receiver) override;

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual bool setPrototype(ExecutionState& state, const Value& value) override;

    virtual Value getPrototype(ExecutionState&) override;
    virtual Object* getPrototypeObject(ExecutionState&) override;

    virtual bool isArray(ExecutionState&) const override;
    virtual bool isExtensible(ExecutionState&) override;

    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override;

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
    virtual Value call(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv) override;
    virtual Object* construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;

    bool m_isCallable : 1;
    bool m_isConstructible : 1;

    Object* m_target;
    Object* m_handler;
};
}

#endif // __EscargotProxyObject__
