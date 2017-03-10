/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotFunctionObject__
#define __EscargotFunctionObject__

#include "parser/CodeBlock.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

extern size_t g_functionObjectTag;

class FunctionObject : public Object {
    friend class GlobalObject;
    enum ForBuiltin { __ForBuiltin__ };
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin);
    void initFunctionObject(ExecutionState& state);
    FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin);

public:
    FunctionObject(ExecutionState& state, NativeFunctionInfo info);
    FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnvironment);

    Value getFunctionPrototype(ExecutionState& state)
    {
        ASSERT(isConstructor());
        return m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER];
    }

    bool setFunctionPrototype(ExecutionState& state, const Value& v)
    {
        ASSERT(isConstructor());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = v;
        return true;
    }

    bool isConstructor()
    {
        return m_codeBlock->isConsturctor();
    }

    virtual bool isFunctionObject() const
    {
        return true;
    }

    CodeBlock* codeBlock()
    {
        return m_codeBlock;
    }

    LexicalEnvironment* outerEnvironment()
    {
        return m_outerEnvironment;
    }

    Value call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false);
    ALWAYS_INLINE static Value call(ExecutionState& state, const Value& callee, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression = false)
    {
        if (LIKELY(callee.isObject() && callee.asPointerValue()->hasTag(g_functionObjectTag))) {
            return callee.asFunction()->call(state, receiver, argc, argv, isNewExpression);
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Function";
    }

protected:
    void generateBytecodeBlock(ExecutionState& state);
    CodeBlock* m_codeBlock;
    LexicalEnvironment* m_outerEnvironment;
};
}

#endif
