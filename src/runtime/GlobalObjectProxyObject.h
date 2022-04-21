/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#ifndef __EscargotGlobalObjectProxyObject__
#define __EscargotGlobalObjectProxyObject__

#include "runtime/GlobalObject.h"
#include "api/EscargotPublic.h"

namespace Escargot {

class GlobalObjectProxyObject : public DerivedObject {
public:
    GlobalObjectProxyObject(ExecutionState& state, GlobalObject* target, GlobalObjectProxyObjectRef::SecurityCheckCallback securityCheckCallback)
        : DerivedObject(state, Object::PrototypeIsNull)
        , m_target(target)
        , m_securityCheckCallback(securityCheckCallback)
    {
    }

    GlobalObject* target()
    {
        return m_target;
    }

    void setTarget(GlobalObject* target)
    {
        m_target = target;
    }

    virtual bool isGlobalObjectProxyObject() const override
    {
        return true;
    }

    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P, const Value& receiver) override;
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override;
    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& propertyName) override;

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override;
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    virtual Value getPrototype(ExecutionState&) override;
    virtual Object* getPrototypeObject(ExecutionState&) override;
    virtual bool setPrototype(ExecutionState& state, const Value& proto) override;

    virtual OwnPropertyKeyVector ownPropertyKeys(ExecutionState& state) override;
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) override;

    virtual bool isExtensible(ExecutionState&) override;
    virtual bool preventExtensions(ExecutionState&) override;

    virtual bool hasOwnEnumeration() const override
    {
        return true;
    }

    virtual bool canUseOwnPropertyKeysFastPath() override
    {
        return false;
    }

    virtual bool isInlineCacheable() override
    {
        return false;
    }

protected:
    GlobalObject* m_target;
    // I use public type directly for performance
    GlobalObjectProxyObjectRef::SecurityCheckCallback m_securityCheckCallback;

    void checkSecurity(ExecutionState& state, GlobalObjectProxyObjectRef::AccessOperationType type, Optional<AtomicString> nonIndexedStringPropertyNameIfExists)
    {
        if (nonIndexedStringPropertyNameIfExists) {
            m_securityCheckCallback((ExecutionStateRef*)&state, (GlobalObjectProxyObjectRef*)this, (GlobalObjectRef*)m_target, type, OptionalRef<AtomicStringRef>(reinterpret_cast<AtomicStringRef*>(nonIndexedStringPropertyNameIfExists.value().string())));
        } else {
            m_securityCheckCallback((ExecutionStateRef*)&state, (GlobalObjectProxyObjectRef*)this, (GlobalObjectRef*)m_target, type, OptionalRef<AtomicStringRef>());
        }
    }
};
} // namespace Escargot

#endif
