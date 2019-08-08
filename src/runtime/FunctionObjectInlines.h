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
    Value operator()(ExecutionState& calleeState, FunctionObject* self, const Value& thisArgument, bool isStrict)
    {
        // OrdinaryCallBindThis ( F, calleeContext, thisArgument )
        // Let thisMode be the value of F’s [[ThisMode]] internal slot.
        // If thisMode is lexical, return NormalCompletion(undefined).
        // --> thisMode is always not lexcial because this is class ctor.
        // Let calleeRealm be the value of F’s [[Realm]] internal slot.
        // Let localEnv be the LexicalEnvironment of calleeContext.
        ASSERT(calleeState.context() == self->codeBlock()->context());

        if (isStrict) {
            // If thisMode is strict, let thisValue be thisArgument.
            return thisArgument;
        } else {
            // Else
            // if thisArgument is null or undefined, then
            // Let thisValue be calleeRealm.[[globalThis]]
            if (thisArgument.isUndefinedOrNull()) {
                return calleeState.context()->globalObject();
            } else {
                // Else
                // Let thisValue be ToObject(thisArgument).
                // Assert: thisValue is not an abrupt completion.
                // NOTE ToObject produces wrapper objects using calleeRealm.
                return thisArgument.toObject(calleeState);
            }
        }
    }
};

class FunctionObjectReturnValueBinder {
public:
    Value operator()(ExecutionState& calleeState, FunctionObject* self, const Value& interpreterReturnValue, const Value& thisArgument, FunctionEnvironmentRecord* record)
    {
        return interpreterReturnValue;
    }
};

class FunctionObjectNewTargetBinder {
public:
    void operator()(ExecutionState& calleeState, FunctionObject* self, Object* newTarget, FunctionEnvironmentRecord* record)
    {
    }
};

class FunctionObjectProcessCallGenerator {
public:
    template <typename FunctionObjectType, bool isConstructCall, bool hasNewTargetOnEnvironment, bool canBindThisValueOnEnvironment, typename ThisValueBinder, typename NewTargetBinder, typename ReturnValueBinder>
    static inline Value processCall(ExecutionState& state, FunctionObjectType* self, const Value& thisArgument, const size_t argc, Value* argv, Object* newTarget) // newTarget is null on [[call]]
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

        CodeBlock* codeBlock = self->codeBlock();
        Context* ctx = codeBlock->context();
        bool isStrict = codeBlock->isStrict();

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
            record = new (alloca(sizeof(FunctionEnvironmentRecord))) FunctionEnvironmentRecordOnStack<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
            lexEnv = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, self->outerEnvironment()
#ifndef NDEBUG
                                                                                             ,
                                                                                 false
#endif
                                                                                 );
        } else {
            if (LIKELY(codeBlock->canUseIndexedVariableStorage())) {
                record = new FunctionEnvironmentRecordOnHeap<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
            } else {
                if (LIKELY(!codeBlock->needsVirtualIDOperation())) {
                    record = new FunctionEnvironmentRecordNotIndexed<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
                } else {
                    record = new FunctionEnvironmentRecordNotIndexedWithVirtualID(self);
                }
            }
            lexEnv = new LexicalEnvironment(record, self->outerEnvironment());
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

        // binding function name
        stackStorage[1] = self;
        if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap())) {
            if (codeBlock->canUseIndexedVariableStorage()) {
                ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                ((FunctionEnvironmentRecordOnHeap<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>*)record)->heapStorage()[0] = self;
            } else {
                record->initializeBinding(state, codeBlock->functionName(), self);
            }
        }

        if (UNLIKELY(codeBlock->isFunctionNameExplicitlyDeclared())) {
            if (codeBlock->canUseIndexedVariableStorage()) {
                if (UNLIKELY(codeBlock->isFunctionNameSaveOnHeap())) {
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>*)record)->heapStorage()[0] = Value();
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
                        ((FunctionEnvironmentRecordOnHeap<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>*)record)->heapStorage()[info[i].m_index] = val;
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

        ThisValueBinder thisValueBinder;
        if (UNLIKELY(codeBlock->isGenerator())) {
            Value* gcArgv = (Value*)GC_MALLOC(sizeof(Value) * argc);
            memcpy(gcArgv, argv, sizeof(Value) * argc);
            ExecutionState* newState = new ExecutionState(ctx, &state, lexEnv, self, argc, gcArgv, isStrict, registerFile);
            // prepare receiver(this variable)

            // we should use newState because
            // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
            // NOTE ToObject produces wrapper objects using calleeRealm. <<----
            stackStorage[0] = thisValueBinder(*newState, self, thisArgument, isStrict);

            if (isConstructCall) {
                NewTargetBinder newTargetBinder;
                newTargetBinder(*newState, self, newTarget, record);
            }

            // FIXME
            if (UNLIKELY(codeBlock->usesArgumentsObject())) {
                bool isMapped = !codeBlock->hasArgumentInitializers() && !isStrict;
                self->generateArgumentsObject(*newState, argc, argv, record, stackStorage, isMapped);
            }

            GeneratorObject* gen = new GeneratorObject(state, newState, blk);
            newState->setGeneratorTarget(gen);
            return gen;
        }

        ExecutionState newState(ctx, &state, lexEnv, self, argc, argv, isStrict);

        // prepare receiver(this variable)
        // we should use newState because
        // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
        // NOTE ToObject produces wrapper objects using calleeRealm. <<----
        stackStorage[0] = thisValueBinder(newState, self, thisArgument, isStrict);

        if (isConstructCall) {
            NewTargetBinder newTargetBinder;
            newTargetBinder(newState, self, newTarget, record);
        }

        if (UNLIKELY(codeBlock->usesArgumentsObject())) {
            // FIXME check if formal parameters does not contain a rest parameter, any binding patterns, or any initializers.
            bool isMapped = !codeBlock->hasArgumentInitializers() && !isStrict;
            self->generateArgumentsObject(newState, argc, argv, record, stackStorage, isMapped);
        }

        // run function
        ReturnValueBinder returnValueBinder;
        const Value returnValue = returnValueBinder(newState, self, ByteCodeInterpreter::interpret(newState, blk, 0, registerFile), thisArgument, record);

        if (UNLIKELY(blk->m_shouldClearStack))
            clearStack<512>();

        return returnValue;
    }
};
}

#endif
