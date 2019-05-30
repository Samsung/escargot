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

extern size_t g_functionObjectTag;

class ArrayObject;
class FunctionEnvironmentRecord;

class FunctionObject : public Object {
    friend class GlobalObject;
    friend class Script;
    void initFunctionObject(ExecutionState& state);

    enum ForGlobalBuiltin { __ForGlobalBuiltin__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin);

public:
    enum ThisMode {
        Lexical,
        Strict,
        Global,
    };

    enum ConstructorKind {
        Derived,
        Base,
    };

    enum FunctionKind {
        Normal,
        ClassConstructor,
        Generator
    };

    FunctionObject(ExecutionState& state, NativeFunctionInfo info);
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment);
    enum ForBind { __ForBind__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, String* functionName, const Value& proto, ForBind);
    enum ForBuiltin { __ForBuiltin__ };
    FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin);
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin);

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

    bool isBuiltin()
    {
        return m_isBuiltin;
    }

    virtual bool isFunctionObject() const override
    {
        return true;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        CodeBlock* cb = m_codeBlock;

        if (UNLIKELY(cb->isBindedFunction())) {
            // for nested bind function
            while (cb->isBindedFunction()) {
                cb = Value(cb->boundFunctionInfo()->m_boundTargetFunction).asFunction()->codeBlock();
            }
        }

        return cb->isConstructor();
    }

    CodeBlock* codeBlock()
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
        return m_constructorKind;
    }

    void setConstructorKind(ConstructorKind kind)
    {
        m_constructorKind = kind;
    }

    FunctionKind functionKind()
    {
        if (isClassConstructor()) {
            return FunctionKind::ClassConstructor;
        }
        /* TODO:
        else if (isGenerator()) {
            return FunctionKind::Generator;
        }
        */

        return FunctionKind::Normal;
    }

    // FIXME : switch to private member function
    Value processCall(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv, bool isNewExpression);

    // https://www.ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-call-thisargument-argumentslist
    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, NULLABLE Value* argv) override
    {
        return processCall(state, thisValue, argc, argv, false);
    }

    virtual Object* construct(ExecutionState& state, const size_t argc, NULLABLE Value* argv, const Value& newTarget) override;

    // https://www.ecma-international.org/ecma-262/6.0/#sec-call
    ALWAYS_INLINE static Value call(ExecutionState& state, const Value& callee, const Value& thisValue, const size_t argc, NULLABLE Value* argv)
    {
        // If IsCallable(F) is false, throw a TypeError exception.
        if (callee.isCallable() == false) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_NOT_Callable);
        }
        // Return F.[[Call]](V, argumentsList).
        return callee.asObject()->call(state, thisValue, argc, argv);
    }

    // https://www.ecma-international.org/ecma-262/6.0/#sec-construct
    ALWAYS_INLINE static Object* construct(ExecutionState& state, const Value& constructor, const size_t argc, NULLABLE Value* argv, Value newTarget = Value(Value::EmptyValue))
    {
        // If newTarget was not passed, let newTarget be F.
        if (newTarget.isEmpty() == true) {
            newTarget = constructor;
        }
        // Assert: IsConstructor (F) is true.
        ASSERT(constructor.isConstructor() == true);
        // Assert: IsConstructor (newTarget) is true.
        ASSERT(newTarget.isConstructor() == true);
        // Return F.[[Construct]](argumentsList, newTarget).
        return constructor.asObject()->construct(state, argc, argv, newTarget);
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty() override
    {
        return "Function";
    }

    bool hasInstance(ExecutionState& state, const Value& O);

    void setHomeObject(Object* homeObject)
    {
        m_homeObject = homeObject;
    }

private:
    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

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

    void generateArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* fnRecord, Value* stackStorage);
    void generateBytecodeBlock(ExecutionState& state);
    CodeBlock* m_codeBlock;
    LexicalEnvironment* m_outerEnvironment;
    Object* m_homeObject;
    ConstructorKind m_constructorKind;
    bool m_isBuiltin : 1;
};
}

#endif
