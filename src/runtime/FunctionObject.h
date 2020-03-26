/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "parser/CodeBlock.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ArrayObject;
class FunctionEnvironmentRecord;
class ScriptFunctionObject;
class ScriptClassConstructorFunctionObject;
class NativeFunctionObject;
class FunctionObjectProcessCallGenerator;

class FunctionObject : public Object {
    friend class Object;
    friend class ObjectGetResult;
    friend class GlobalObject;
    friend class Script;

public:
    enum ThisMode {
        Lexical,
        Strict,
        Global,
    };

    enum ConstructorKind {
        Base,
        Derived,
    };

    // getter prototype property of constructible function(not [[prototype]]!!!)
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    Value getFunctionPrototype(ExecutionState& state)
    {
        if (LIKELY(isConstructor() || isGenerator())) {
            auto idx = functionPrototypeIndex();
            ensureFunctionPrototype(state, idx);
            return m_values[idx];
        } else {
            return Value();
        }
    }

    // getter prototype property constructible function(not [[prototype]]!!!)
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    bool setFunctionPrototype(ExecutionState& state, const Value& v)
    {
        if (LIKELY(isConstructor() || isGenerator())) {
            m_values[functionPrototypeIndex()] = v;
            return true;
        } else {
            return false;
        }
    }

    bool isArrowFunction()
    {
        return m_codeBlock->isArrowFunctionExpression();
    }

    bool isClassConstructor()
    {
        return m_codeBlock->isClassConstructor();
    }

    bool isGenerator()
    {
        return m_codeBlock->isGenerator();
    }

    virtual bool isFunctionObject() const override
    {
        return true;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    CodeBlock* codeBlock() const
    {
        return m_codeBlock;
    }

    ThisMode thisMode()
    {
        if (isArrowFunction()) {
            return ThisMode::Lexical;
        } else if (m_codeBlock->isStrict()) {
            return ThisMode::Strict;
        } else {
            return ThisMode::Global;
        }
    }

    ConstructorKind constructorKind()
    {
        return codeBlock()->isDerivedClassConstructor() ? ConstructorKind::Derived : ConstructorKind::Base;
    }

    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects
    // internal [[homeObject]] slot
    virtual Object* homeObject()
    {
        return nullptr;
    }

    virtual Context* getFunctionRealm(ExecutionState& state) override
    {
        return m_codeBlock->context();
    }

    struct FunctionSource {
        Script* script;
        CodeBlock* codeBlock;
        LexicalEnvironment* outerEnvironment;
    };
    static FunctionSource createFunctionSourceFromScriptSource(ExecutionState& state, AtomicString functionName, size_t argumentValueArrayCount, Value* argumentValueArray, Value bodyString, bool useStrict, bool isGenerator, bool isAsync, bool allowSuperCall);

protected:
    FunctionObject(ExecutionState& state, Object* proto, size_t defaultSpace); // function for derived classes. derived class MUST initlize member variable of FunctionObject.

    void initStructureAndValues(ExecutionState& state, bool isConstructor, bool isGenerator, bool isAsync);
    virtual size_t functionPrototypeIndex()
    {
        ASSERT(isConstructor() || isGenerator());
        return ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER;
    }

    void ensureFunctionPrototype(ExecutionState& state, const size_t& prototypeIndex)
    {
        if (m_values[prototypeIndex].isEmpty()) {
            m_values[prototypeIndex] = createFunctionPrototypeObject(state);
        }
    }

    virtual Object* createFunctionPrototypeObject(ExecutionState& state)
    {
        return Object::createFunctionPrototypeObject(state, this);
    }

    static inline void fillGCDescriptor(GC_word* desc)
    {
        Object::fillGCDescriptor(desc);

        GC_set_bit(desc, GC_WORD_OFFSET(FunctionObject, m_codeBlock));
    }

    CodeBlock* m_codeBlock;
};
}

#endif
