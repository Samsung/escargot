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

class FunctionEnvironmentRecord;

class FunctionObject : public Object {
    friend class GlobalObject;
    friend class Script;
    void initFunctionObject(ExecutionState& state);

    enum ForGlobalBuiltin { __ForGlobalBuiltin__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForGlobalBuiltin);

public:
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

    virtual bool isFunctionObject() const
    {
        return true;
    }

    virtual bool isCallable() const override
    {
        return true;
    }

    virtual bool isConstructor() const override
    {
        return m_codeBlock->isConstructor();
    }

    CodeBlock* codeBlock()
    {
        return m_codeBlock;
    }

    Value call(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv)
    {
        return processCall(state, receiver, argc, argv, false);
    }
    // ECMAScript new operation
    Object* newInstance(ExecutionState& state, const size_t argc, Value* argv);

    ALWAYS_INLINE static Value call(ExecutionState& state, const Value& callee, const Value& receiver, const size_t argc, Value* argv, bool isNewExpression = false)
    {
        if (LIKELY(callee.isObject() && callee.asPointerValue()->hasTag(g_functionObjectTag))) {
            return callee.asFunction()->processCall(state, receiver, argc, argv, isNewExpression);
        } else {
            return callSlowCase(state, callee, receiver, argc, argv, isNewExpression);
        }
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Function";
    }

    bool hasInstance(ExecutionState& state, const Value& O);

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

    Value processCall(ExecutionState& state, const Value& receiver, const size_t argc, Value* argv, bool isNewExpression);
    static Value callSlowCase(ExecutionState& state, const Value& callee, const Value& receiver, const size_t argc, Value* argv, bool isNewExpression);
    void generateArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* fnRecord, Value* stackStorage);
    void generateBytecodeBlock(ExecutionState& state);
    CodeBlock* m_codeBlock;
    LexicalEnvironment* m_outerEnvironment;
};
}

#endif
