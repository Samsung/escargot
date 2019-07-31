/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "parser/CodeBlock.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

class ArrayObject;
class FunctionEnvironmentRecord;
class ScriptFunctionObject;
class NativeFunctionObject;

class FunctionObjectProcessCallGenerator {
public:
    template <typename FunctionObjectType, bool isConstructCall, typename ThisValueBinder>
    static inline Value processCall(ExecutionState& state, FunctionObjectType* self, const Value& receiver, const size_t argc, Value* argv);
};

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

    enum FunctionKind {
        Normal,
        ClassConstructor,
        Generator
    };

    // getter of internal [[Prototype]]
    Value getFunctionPrototype(ExecutionState& state)
    {
        if (LIKELY(isConstructor()))
            return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER];
        else
            return Value();
    }

    // setter of internal [[Prototype]]
    bool setFunctionPrototype(ExecutionState& state, const Value& v)
    {
        if (LIKELY(isConstructor())) {
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = v;
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

    virtual bool isBuiltin()
    {
        return false;
    }

    virtual bool isFunctionObject() const override
    {
        return true;
    }

    virtual bool isNativeFunctionObject() const
    {
        return false;
    }

    NativeFunctionObject* asNativeFunctionObject()
    {
        ASSERT(isNativeFunctionObject());
        return (NativeFunctionObject*)this;
    }

    virtual bool isScriptFunctionObject() const
    {
        return false;
    }

    ScriptFunctionObject* asScriptFunctionObject()
    {
        ASSERT(isScriptFunctionObject());
        return (ScriptFunctionObject*)this;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        CodeBlock* cb = m_codeBlock;
        return cb->isConstructor();
    }

    CodeBlock* codeBlock() const
    {
        return m_codeBlock;
    }

    Object* homeObject()
    {
        return m_homeObject;
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
        return codeBlock()->isDerivedClassConstructor() == ConstructorKind::Derived ? ConstructorKind::Derived : ConstructorKind::Base;
    }

    FunctionKind functionKind()
    {
        if (isClassConstructor()) {
            return FunctionKind::ClassConstructor;
        } else if (isGenerator()) {
            return FunctionKind::Generator;
        }

        return FunctionKind::Normal;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Function";
    }

    void setHomeObject(Object* homeObject)
    {
        m_homeObject = homeObject;
    }

protected:
    FunctionObject(ExecutionState& state, size_t defaultSpace); // function for derived classes. derived class MUST initlize member variable of FunctionObject.

    void initStructureAndValues(ExecutionState& state);

    Value getFunctionPrototypeKnownAsConstructor(ExecutionState& state)
    {
        ASSERT(isConstructor());
        return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER];
    }

    bool setFunctionPrototypeKnownAsConstructor(ExecutionState& state, const Value& v)
    {
        ASSERT(isConstructor());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = v;
        return true;
    }
    CodeBlock* m_codeBlock;
    Object* m_homeObject;
};

class ScriptFunctionObject : public FunctionObject {
    friend class Script;

public:
    ScriptFunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment);

protected:
    ScriptFunctionObject(ExecutionState& state, size_t defaultSpace); // function for derived classes. derived class MUST initlize member variable of FunctionObject.

    friend class FunctionObjectProcessCallGenerator;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-call-thisargument-argumentslist
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget) override;

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    virtual bool isScriptFunctionObject() const override
    {
        return true;
    }

    void generateArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* fnRecord, Value* stackStorage, bool isMapped);
    void generateRestParameter(ExecutionState& state, FunctionEnvironmentRecord* record, Value* parameterStorageInStack, const size_t argc, Value* argv);
    void generateByteCodeBlock(ExecutionState& state);

    LexicalEnvironment* m_outerEnvironment;
};
}

#endif
