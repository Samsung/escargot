/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotModuleNamespaceObject__
#define __EscargotModuleNamespaceObject__

#include "runtime/Object.h"

namespace Escargot {

class ModuleEnvironmentRecord;

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects
class ModuleNamespaceObject : public Object {
public:
    ModuleNamespaceObject(ExecutionState& state, ModuleEnvironmentRecord* record);

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getprototypeof
    virtual Object* getPrototypeObject(ExecutionState&) override
    {
        return nullptr;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getprototypeof
    virtual Value getPrototype(ExecutionState&) override
    {
        return Value(Value::Null);
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-setprototypeof-v
    virtual bool setPrototype(ExecutionState&, const Value&) override
    {
        return false;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-isextensible
    virtual bool isExtensible(ExecutionState&) override
    {
        if (!m_isInitialized) {
            return true;
        }
        return false;
    }

    virtual bool preventExtensions(ExecutionState&) override
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-defineownproperty-p-desc
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override
    {
        return false;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-hasproperty-p
    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-get-p-receiver
    virtual ObjectGetResult get(ExecutionState& state, const ObjectPropertyName& P) override;

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-set-p-v-receiver
    virtual bool set(ExecutionState& state, const ObjectPropertyName& P, const Value& v, const Value& receiver) override
    {
        return false;
    }

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-delete-p
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-ownpropertykeys
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) override;

private:
    bool m_isInitialized;
    ModuleEnvironmentRecord* m_moduleEnvironmentRecord;
    AtomicStringVector m_exports;
};
}

#endif
