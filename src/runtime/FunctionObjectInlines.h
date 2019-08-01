/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotFunctionObjectInlines__
#define __EscargotFunctionObjectInlines__

// don't include this file except FunctionObject and descendants of FunctionObject
#include "runtime/Context.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ArrayObject.h"
#include "runtime/GeneratorObject.h"
#include "util/Util.h"

namespace Escargot {

// this is default version of ThisValueBinder
class FunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& state, Context* context, FunctionObject* self, const Value& receiverSrc, bool isStrict)
    {
        if (!isStrict) {
            if (receiverSrc.isUndefinedOrNull()) {
                return context->globalObject();
            } else {
                return receiverSrc.toObject(state);
            }
        } else {
            return receiverSrc;
        }
    }
};

template <typename FunctionObjectType, bool isConstructCall, typename ThisValueBinder>
Value FunctionObjectProcessCallGenerator::processCall(ExecutionState& state, FunctionObjectType* self, const Value& receiverSrc, const size_t argc, Value* argv)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY((state.stackBase() - currentStackBase) > STACK_LIMIT_FROM_BASE)) {
#else
    if (UNLIKELY((currentStackBase - state.stackBase()) > STACK_LIMIT_FROM_BASE)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    ASSERT(self->codeBlock()->isInterpretedCodeBlock());
    ASSERT(!self->isBuiltin());

    CodeBlock* codeBlock = self->codeBlock();
    Context* ctx = codeBlock->context();
    bool isStrict = codeBlock->isStrict();
    bool isSuperCall = state.isOnGoingSuperCall();

    if (UNLIKELY(!isConstructCall && self->functionKind() == FunctionObject::FunctionKind::ClassConstructor && !isSuperCall)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Class constructor cannot be invoked without 'new'");
    }

    // prepare ByteCodeBlock if needed
    if (UNLIKELY(codeBlock->asInterpretedCodeBlock()->byteCodeBlock() == nullptr)) {
        self->generateByteCodeBlock(state);
    }

    ByteCodeBlock* blk = codeBlock->asInterpretedCodeBlock()->byteCodeBlock();

    size_t registerSize = blk->m_requiredRegisterFileSizeInValueSize;
    size_t identifierOnStackCount = codeBlock->asInterpretedCodeBlock()->identifierOnStackCount();
    size_t stackStorageSize = codeBlock->asInterpretedCodeBlock()->totalStackAllocatedVariableSize();
    size_t literalStorageSize = blk->m_numeralLiteralData.size();
    Value* literalStorageSrc = blk->m_numeralLiteralData.data();
    size_t parameterCopySize = std::min(argc, (size_t)codeBlock->parameterCount());

    // prepare env, ec
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* lexEnv;

    if (LIKELY(codeBlock->canAllocateEnvironmentOnStack())) {
        // no capture, very simple case
        record = new (alloca(sizeof(FunctionEnvironmentRecordSimple))) FunctionEnvironmentRecordSimple(self);
        lexEnv = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, self->outerEnvironment());
    } else {
        if (LIKELY(codeBlock->canUseIndexedVariableStorage())) {
            record = new FunctionEnvironmentRecordOnHeap(self, argc, argv);
        } else {
            if (LIKELY(!codeBlock->needsVirtualIDOperation())) {
                record = new FunctionEnvironmentRecordNotIndexed(self, argc, argv);
            } else {
                record = new FunctionEnvironmentRecordNotIndexedWithVirtualID(self, argc, argv);
            }
        }
        lexEnv = new LexicalEnvironment(record, self->outerEnvironment());
    }

    if (receiverSrc.isObject()) {
        record->setNewTarget(receiverSrc.asObject());
    }

    Value* registerFile;

    if (UNLIKELY(codeBlock->isGenerator())) {
        if (isConstructCall) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Generator cannot be invoked with 'new'");
        }
        registerFile = (Value*)GC_MALLOC((registerSize + stackStorageSize + literalStorageSize) * sizeof(Value));
    } else {
        registerFile = (Value*)alloca((registerSize + stackStorageSize + literalStorageSize) * sizeof(Value));
    }

    Value* stackStorage = registerFile + registerSize;

    {
        Value* literalStorage = stackStorage + stackStorageSize;
        for (size_t i = 0; i < literalStorageSize; i++) {
            literalStorage[i] = literalStorageSrc[i];
        }
    }

    // prepare receiver(this variable)
    ThisValueBinder thisValueBinder;
    stackStorage[0] = thisValueBinder(state, ctx, self, receiverSrc, isStrict);

    if (self->constructorKind() == FunctionObject::ConstructorKind::Base && self->thisMode() != FunctionObject::ThisMode::Lexical) {
        record->bindThisValue(state, stackStorage[0]);
    }

    // binding function name
    stackStorage[1] = self;
    if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap())) {
        if (codeBlock->canUseIndexedVariableStorage()) {
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            ((FunctionEnvironmentRecordOnHeap*)record)->heapStorage()[0] = self;
        } else {
            record->initializeBinding(state, codeBlock->functionName(), self);
        }
    }

    if (UNLIKELY(codeBlock->isFunctionNameExplicitlyDeclared())) {
        if (codeBlock->canUseIndexedVariableStorage()) {
            if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap())) {
                ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                ((FunctionEnvironmentRecordOnHeap*)record)->heapStorage()[0] = Value();
            } else {
                stackStorage[1] = Value();
            }
        } else {
            record->initializeBinding(state, codeBlock->functionName(), Value());
        }
    }

    // prepare parameters
    if (UNLIKELY(codeBlock->needsComplexParameterCopy())) {
        for (size_t i = 2; i < identifierOnStackCount; i++) {
            stackStorage[i] = Value();
        }

#ifndef NDEBUG
        for (size_t i = identifierOnStackCount; i < stackStorageSize; i++) {
            stackStorage[i] = Value(Value::EmptyValue);
        }
#endif
        if (!codeBlock->canUseIndexedVariableStorage()) {
            const InterpretedCodeBlock::FunctionParametersInfoVector& info = codeBlock->asInterpretedCodeBlock()->parametersInfomation();
            for (size_t i = 0; i < parameterCopySize; i++) {
                record->initializeBinding(state, info[i].m_name, argv[i]);
            }
            for (size_t i = parameterCopySize; i < info.size(); i++) {
                record->initializeBinding(state, info[i].m_name, Value());
            }
        } else {
            Value* parameterStorageInStack = stackStorage + 2;
            const InterpretedCodeBlock::FunctionParametersInfoVector& info = codeBlock->asInterpretedCodeBlock()->parametersInfomation();
            for (size_t i = 0; i < info.size(); i++) {
                Value val(Value::ForceUninitialized);
                // NOTE: consider the special case with duplicated parameter names (**test262: S10.2.1_A3)
                if (i < argc)
                    val = argv[i];
                else if (info[i].m_index >= (int32_t)argc)
                    continue;
                else
                    val = Value();
                if (info[i].m_isHeapAllocated) {
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)record)->heapStorage()[info[i].m_index] = val;
                } else {
                    parameterStorageInStack[info[i].m_index] = val;
                }
            }
        }
    } else {
        Value* parameterStorageInStack = stackStorage + 2;
        for (size_t i = 0; i < parameterCopySize; i++) {
            parameterStorageInStack[i] = argv[i];
        }

        for (size_t i = parameterCopySize + 2; i < identifierOnStackCount; i++) {
            stackStorage[i] = Value();
        }

#ifndef NDEBUG
        for (size_t i = identifierOnStackCount; i < stackStorageSize; i++) {
            stackStorage[i] = Value(Value::EmptyValue);
        }
#endif
    }

    if (UNLIKELY(codeBlock->isGenerator())) {
        ExecutionState* newState = new ExecutionState(ctx, &state, lexEnv, isStrict, registerFile);

        // FIXME
        if (UNLIKELY(codeBlock->usesArgumentsObject())) {
            bool isMapped = !codeBlock->usesRestParameter() && !isStrict;
            self->generateArgumentsObject(*newState, record, stackStorage, isMapped);
        }

        GeneratorObject* gen = new GeneratorObject(state, newState, blk);
        newState->setGeneratorTarget(gen);
        return gen;
    }

    ExecutionState newState(ctx, &state, lexEnv, isStrict, registerFile);

    if (UNLIKELY(codeBlock->usesArgumentsObject())) {
        // FIXME check if formal parameters does not contain a rest parameter, any binding patterns, or any initializers.
        bool isMapped = !codeBlock->usesRestParameter() && !isStrict;
        self->generateArgumentsObject(newState, record, stackStorage, isMapped);
    }

    if (UNLIKELY(codeBlock->usesRestParameter())) {
        self->generateRestParameter(newState, record, stackStorage + 2, argc, argv);
    }

    // run function
    const Value returnValue = ByteCodeInterpreter::interpret(newState, blk, 0, registerFile);
    if (UNLIKELY(blk->m_shouldClearStack))
        clearStack<512>();

    if (UNLIKELY(isSuperCall)) {
        if (returnValue.isNull()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InvalidDerivedConstructorReturnValue);
        }

        FunctionEnvironmentRecord* thisEnvironmentRecord = state.getThisEnvironment()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
        if (returnValue.isObject()) {
            thisEnvironmentRecord->bindThisValue(state, returnValue);
            returnValue.asObject()->setPrototype(state, receiverSrc.toObject(state)->getPrototype(state));
        } else {
            thisEnvironmentRecord->bindThisValue(state, stackStorage[0]);
        }
        state.setOnGoingSuperCall(false);
    }

    return returnValue;
}
}

#endif
