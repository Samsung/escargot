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
#include "runtime/PrototypeObject.h"

namespace Escargot {

class ScriptClassConstructorFunctionObject;

class ScriptClassConstructorPrototypeObject : public PrototypeObject {
public:
    friend class ScriptClassConstructorFunctionObject;
    explicit ScriptClassConstructorPrototypeObject(ExecutionState& state)
        : PrototypeObject(state)
        , m_constructor(nullptr)
    {
    }

    virtual bool isScriptClassConstructorPrototypeObject() const override
    {
        return true;
    }

    ScriptClassConstructorFunctionObject* constructor() const
    {
        return m_constructor;
    }

private:
    ScriptClassConstructorFunctionObject* m_constructor;
};

class ScriptClassConstructorFunctionObject : public ScriptFunctionObject {
    friend class ScriptClassConstructorFunctionObjectThisValueBinder;
    friend class InterpreterSlowPath;

public:
    ScriptClassConstructorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment,
                                         Object* homeObject, Optional<Object*> outerClassConstructor, String* classSourceCode, const Optional<AtomicString>& name);

    friend class FunctionObjectProcessCallGenerator;
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override;
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;

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

    Optional<Object*> outerClassConstructor()
    {
        return m_outerClassConstructor;
    }

    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    enum ClassPrivateFieldKind {
        NotPrivate,
        PrivateFieldValue,
        PrivateFieldMethod,
        PrivateFieldGetter,
        PrivateFieldSetter,
    };

private:
    void initInstanceFieldMembers(ExecutionState& state, Object* instance);

    virtual size_t functionPrototypeIndex() override
    {
        return m_prototypeIndex;
    }

    size_t m_prototypeIndex;
    Object* m_homeObject;
    Optional<Object*> m_outerClassConstructor;
    // We needs to store class source code for toString(). because class constructor stores its source code
    String* m_classSourceCode;
    TightVector<std::tuple<EncodedValue, EncodedValue, size_t>, GCUtil::gc_malloc_allocator<std::tuple<EncodedValue, EncodedValue, size_t>>> m_instanceFieldInitData;
    VectorWithNoSize<std::tuple<EncodedValue, size_t>, GCUtil::gc_malloc_allocator<std::tuple<EncodedValue, size_t>>> m_staticFieldInitData;
};
} // namespace Escargot

#endif
