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
#include "parser/ScriptParser.h"

namespace Escargot {

class ArrayObject;
class FunctionEnvironmentRecord;
class ScriptFunctionObject;
class ScriptClassConstructorFunctionObject;
class NativeFunctionObject;
class FunctionObjectProcessCallGenerator;

class FunctionObject : public DerivedObject {
    friend class Object;
    friend class ObjectGetResult;
    friend class GlobalObject;
    friend class Script;

public:
    enum ConstructorKind {
        Base,
        Derived,
    };

    // get prototype property of constructible function(not [[prototype]]!!!)
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

    // set prototype property constructible function(not [[prototype]]!!!)
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

    virtual bool isGenerator() const
    {
        return false;
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
        InterpretedCodeBlock* codeBlock;
        LexicalEnvironment* outerEnvironment;
    };

    // create a script for dynamic function
    static FunctionSource createDynamicFunctionScript(ExecutionState& state, AtomicString functionName, size_t argCount, Value* argArray, Value bodyString, bool useStrict, bool isGenerator, bool isAsync, bool allowSuperCall, bool isInternalSource = false, String* sourceName = String::emptyString());
    // create a general function script which is not a dynamic function
    static ScriptParser::InitializeScriptResult createFunctionScript(ExecutionState& state, String* sourceName, AtomicString functionName, size_t argCount, Value* argArray, Value bodyString, bool useStrict);

    bool setName(AtomicString name);

protected:
    FunctionObject(ExecutionState& state, Object* proto, size_t defaultSpace); // function for derived classes. derived class MUST initlize member variable of FunctionObject.
    FunctionObject(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto); // ctor for FunctionTemplate
    FunctionObject() // ctor for reading tag
        : DerivedObject()
        , m_codeBlock(nullptr)
    {
    }

    void initStructureAndValues(ExecutionState& state, bool isConstructor, bool isGenerator);
    virtual size_t functionPrototypeIndex()
    {
        ASSERT(isConstructor() || isGenerator());
        return ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER;
    }

    size_t functionNameIndex()
    {
        // getting a function name index of ScriptClassConstructorFunctionObject is not supported now
        ASSERT(!isScriptClassConstructorFunctionObject());
        if (isConstructor() || isGenerator()) {
            return ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2;
        } else {
            return ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1;
        }
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
} // namespace Escargot

#endif
