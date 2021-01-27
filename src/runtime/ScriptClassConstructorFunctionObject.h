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

#ifndef __EscargotScriptClassConstructorFunctionObject__
#define __EscargotScriptClassConstructorFunctionObject__

#include "runtime/ScriptFunctionObject.h"

namespace Escargot {

class ScriptClassConstructorPrototypeObject : public Object {
public:
    explicit ScriptClassConstructorPrototypeObject(ExecutionState& state)
        : Object(state)
    {
    }

    virtual bool isScriptClassConstructorPrototypeObject() const override
    {
        return true;
    }
};

class ScriptClassConstructorFunctionObject : public ScriptFunctionObject {
    friend class ScriptClassConstructorFunctionObjectThisValueBinder;
    friend class ByteCodeInterpreter;

public:
    ScriptClassConstructorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment, Object* homeObject, String* classSourceCode, const Optional<AtomicString>& name);

    friend class FunctionObjectProcessCallGenerator;
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    virtual Value construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, Object* newTarget) override;

    bool isConstructor() const override
    {
        return true;
    }

    virtual bool isScriptClassConstructorFunctionObject() const override
    {
        return true;
    }

    virtual Object* homeObject() override
    {
        return m_homeObject;
    }

    String* classSourceCode()
    {
        return m_classSourceCode;
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

private:
    void initInstanceFieldMembers(ExecutionState& state, Object* instance);

    virtual size_t functionPrototypeIndex() override
    {
        return m_prototypeIndex;
    }

    size_t m_prototypeIndex;
    Object* m_homeObject;
    // We needs to store class source code for toString(). because class constructor stores its source code
    String* m_classSourceCode;
    TightVector<std::pair<EncodedValue, EncodedValue>, GCUtil::gc_malloc_allocator<std::pair<EncodedValue, EncodedValue>>> m_instanceFieldInitData;
    VectorWithNoSize<EncodedValue, GCUtil::gc_malloc_allocator<EncodedValue>> m_staticFieldInitData;
};
} // namespace Escargot

#endif
