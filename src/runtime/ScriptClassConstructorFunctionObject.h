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
class ClassSourceText;

class ScriptClassConstructorPrototypeObject : public PrototypeObject {
public:
    friend class ScriptClassConstructorFunctionObject;
    explicit ScriptClassConstructorPrototypeObject(ExecutionState& state)
        : PrototypeObject(state)
        , m_constructor(nullptr)
    {
    }
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    virtual bool isScriptClassConstructorPrototypeObject() const override
    {
        return true;
    }

    ScriptClassConstructorFunctionObject* constructor() const
    {
        return m_constructor;
    }

private:
    CompressibleHeapPointer<ScriptClassConstructorFunctionObject> m_constructor;
};

class ScriptClassConstructorFunctionObject : public ScriptFunctionObject {
    friend class ScriptClassConstructorFunctionObjectThisValueBinder;
    friend class InterpreterSlowPath;

public:
    ScriptClassConstructorFunctionObject(ExecutionState& state, Object* proto, InterpretedCodeBlock* codeBlock, LexicalEnvironment* outerEnvironment,
                                         Object* homeObject, Optional<Object*> outerClassConstructor, ClassSourceText* classSourceCode, const Optional<AtomicString>& name);
    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    friend class FunctionObjectProcessCallGenerator;
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override;
    virtual Value construct(ExecutionState& state, const size_t argc, Value* argv, Object* newTarget) override;
    virtual void callConstructor(ExecutionState& state, Object* receiver, const size_t argc, Value* argv, Object* newTarget) override;
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

    String* classSourceCode();

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
    CompressibleHeapPointer<Object> m_homeObject;
    CompressibleHeapPointer<Object> m_outerClassConstructor;
    CompressibleHeapPointer<ClassSourceText> m_classSourceCode;
    CompressibleHeapVectorOwner<TightVector<std::tuple<EncodedValue, EncodedValue, size_t>, GCUtil::gc_malloc_allocator<std::tuple<EncodedValue, EncodedValue, size_t>>>, std::tuple<EncodedValue, EncodedValue, size_t>> m_instanceFieldInitData;
    CompressibleHeapVectorOwner<VectorWithNoSize<std::tuple<EncodedValue, size_t>, GCUtil::gc_malloc_allocator<std::tuple<EncodedValue, size_t>>>, std::tuple<EncodedValue, size_t>> m_staticFieldInitData;
};
} // namespace Escargot

#endif
