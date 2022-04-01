/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotScriptSimpleFunctionObject__
#define __EscargotScriptSimpleFunctionObject__

#include "runtime/ScriptFunctionObject.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/VMInstance.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

template <bool isStrict = false, bool shouldClearStack = false, unsigned registerFileSize = 12>
class ScriptSimpleFunctionObject : public ScriptFunctionObject {
    friend class ScriptFunctionObject;

protected:
    ScriptSimpleFunctionObject() // ctor for reading tag
        : ScriptFunctionObject()
    {
    }

    virtual bool isScriptSimpleFunctionObject() const override
    {
        return true;
    }

    virtual Value call(ExecutionState& state, const Value& thisValue, const size_t argc, Value* argv) override
    {
#ifdef STACK_GROWS_DOWN
        if (UNLIKELY(state.stackLimit() > (size_t)currentStackPointer())) {
#else
        if (UNLIKELY(state.stackLimit() < (size_t)currentStackPointer())) {
#endif
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
        }

        ASSERT(codeBlock()->isInterpretedCodeBlock());
        InterpretedCodeBlock* codeBlock = interpretedCodeBlock();

        // prepare ByteCodeBlock if needed
        if (UNLIKELY(codeBlock->byteCodeBlock() == nullptr)) {
            generateByteCodeBlock(state);
        }

        ByteCodeBlock* blk = codeBlock->byteCodeBlock();
        Context* ctx = codeBlock->context();
        const size_t registerSize = blk->m_requiredOperandRegisterNumber;

#if !defined(NDEBUG)
        const size_t stackStorageSize = codeBlock->totalStackAllocatedVariableSize();
        const size_t literalStorageSize = blk->m_numeralLiteralData.size();
        ASSERT(codeBlock->isStrict() == isStrict);
        ASSERT(blk->m_requiredOperandRegisterNumber + stackStorageSize + literalStorageSize <= registerFileSize);
#endif

        // prepare env, ec
        ASSERT(codeBlock->canAllocateEnvironmentOnStack());
        FunctionEnvironmentRecordOnStack<false, false> record(this);
        LexicalEnvironment lexEnv(&record, outerEnvironment()
#ifndef NDEBUG
                                               ,
                                  false
#endif
        );

        char* registerFileBuffer[sizeof(Value) * registerFileSize];
        Value* registerFile = reinterpret_cast<Value*>(registerFileBuffer);
        Value* stackStorage = registerFile + registerSize;

        ExecutionState newState(ctx, &state, &lexEnv, argc, argv, isStrict);
        if (isStrict) {
            stackStorage[0] = thisValue;
        } else {
            if (thisValue.isUndefinedOrNull()) {
                stackStorage[0] = newState.context()->globalObjectProxy();
            } else {
                stackStorage[0] = thisValue.toObject(newState);
            }
        }

        if (shouldClearStack) {
            const Value returnValue = ByteCodeInterpreter::interpret(&newState, blk, 0, registerFile);
            clearStack<512>();
            return returnValue;
        } else {
            return ByteCodeInterpreter::interpret(&newState, blk, 0, registerFile);
        }
    }
};

COMPILE_ASSERT(sizeof(ScriptSimpleFunctionObject<>) == sizeof(ScriptFunctionObject), "");

} // namespace Escargot

#endif
