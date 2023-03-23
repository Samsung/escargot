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
#include "runtime/AsyncGeneratorObject.h"
#include "runtime/ExecutionPauser.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

class ScriptGeneratorFunctionObject;
class ScriptAsyncFunctionObject;

// this is default version of ThisValueBinder
class FunctionObjectThisValueBinder {
public:
    Value operator()(ExecutionState& callerState, ExecutionState& calleeState, FunctionObject* self, const Value& thisArgument, bool isStrict)
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
                return calleeState.context()->globalObjectProxy();
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
    Value operator()(ExecutionState& callerState, ExecutionState& calleeState, FunctionObject* self, const Value& interpreterReturnValue, const Value& thisArgument, FunctionEnvironmentRecord* record)
    {
        return interpreterReturnValue;
    }
};

class FunctionObjectNewTargetBinder {
public:
    void operator()(ExecutionState& callerState, ExecutionState& calleeState, FunctionObject* self, Object* newTarget, FunctionEnvironmentRecord* record)
    {
    }
};

class FunctionObjectProcessCallGenerator {
public:
    template <typename FunctionObjectType, bool isConstructCall, bool hasNewTargetOnEnvironment, bool canBindThisValueOnEnvironment, typename ThisValueBinder, typename NewTargetBinder, typename ReturnValueBinder>
    static ALWAYS_INLINE Value processCall(ExecutionState& state, FunctionObjectType* self, const Value& thisArgument, const size_t argc, Value* argv, Object* newTarget) // newTarget is null on [[call]]
    {
#ifdef STACK_GROWS_DOWN
        if (UNLIKELY(state.stackLimit() > (size_t)currentStackPointer())) {
#else
        if (UNLIKELY(state.stackLimit() < (size_t)currentStackPointer())) {
#endif
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Maximum call stack size exceeded");
        }

        ASSERT(self->codeBlock()->isInterpretedCodeBlock());
        InterpretedCodeBlock* codeBlock = self->interpretedCodeBlock();

        // prepare ByteCodeBlock if needed
        if (UNLIKELY(codeBlock->byteCodeBlock() == nullptr)) {
            self->generateByteCodeBlock(state);
        }

        ByteCodeBlock* blk = codeBlock->byteCodeBlock();
        Context* ctx = codeBlock->context();
        bool isStrict = codeBlock->isStrict();
        const size_t registerFileSize = blk->m_requiredTotalRegisterNumber;
        const size_t generalRegisterSize = blk->m_requiredOperandRegisterNumber;

        // prepare env, ec
        FunctionEnvironmentRecord* record;
        LexicalEnvironment* lexEnv;

        if (LIKELY(codeBlock->canAllocateEnvironmentOnStack())) {
            // no capture, very simple case
            record = new (alloca(sizeof(FunctionEnvironmentRecordOnStack<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>))) FunctionEnvironmentRecordOnStack<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
            lexEnv = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, self->outerEnvironment()
#ifndef NDEBUG
                                                                                             ,
                                                                                 false
#endif
            );
        } else {
            record = createFunctionEnvironmentRecord<FunctionObjectType, hasNewTargetOnEnvironment, canBindThisValueOnEnvironment>(state, self, codeBlock);
            lexEnv = new LexicalEnvironment(record, self->outerEnvironment());
        }

        Value* registerFile;

        if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncGeneratorFunctionObject>::value) {
            registerFile = (Value*)CustomAllocator<Value>().allocate(registerFileSize);
        } else {
            registerFile = (Value*)alloca((registerFileSize) * sizeof(Value));
        }

        Value* stackStorage = registerFile + generalRegisterSize;

        ThisValueBinder thisValueBinder;
        if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncGeneratorFunctionObject>::value) {
            Value* arguments = CustomAllocator<Value>().allocate(argc);
            memcpy(arguments, argv, sizeof(Value) * argc);

            ExecutionState* newState = new ExecutionState(ctx, nullptr, lexEnv, argc, arguments, isStrict, ExecutionState::ForPauser);

            // prepare receiver(this variable)
            // we should use newState because
            // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
            // NOTE ToObject produces wrapper objects using calleeRealm. <<----
            stackStorage[0] = thisValueBinder(state, *newState, self, thisArgument, isStrict);
            // binding function name
            stackStorage[1] = self;

            if (isConstructCall) {
                NewTargetBinder newTargetBinder;
                newTargetBinder(state, *newState, self, newTarget, record);
            }

            // https://www.ecma-international.org/ecma-262/10.0/#sec-ordinarycreatefromconstructor
            // OrdinaryCreateFromConstructor ( constructor, intrinsicDefaultProto [ , internalSlotsList ] )
            // Assert: intrinsicDefaultProto is a String value that is this specification's name of an intrinsic object. The corresponding object must be an intrinsic that is intended to be used as the [[Prototype]] value of an object.
            // let proto be ? GetPrototypeFromConstructor(constructor, intrinsicDefaultProto).
            Object* proto = nullptr;
            if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value) {
                proto = Object::getPrototypeFromConstructor(state, self, [](ExecutionState& state, Context* constructorRealm) -> Object* {
                    return constructorRealm->globalObject()->generatorPrototype();
                });
            } else {
                proto = Object::getPrototypeFromConstructor(state, self, [](ExecutionState& state, Context* constructorRealm) -> Object* {
                    return constructorRealm->globalObject()->asyncGeneratorPrototype();
                });
            }

            // Return ObjectCreate(proto, internalSlotsList).
            Object* generatorObject;
            if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value) {
                GeneratorObject* gen = new GeneratorObject(state, proto, newState, registerFile, blk);
                newState->setPauseSource(gen->executionPauser());
                ExecutionPauser::start(state, newState->pauseSource(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::Generator);
                generatorObject = gen;
            } else {
                AsyncGeneratorObject* gen = new AsyncGeneratorObject(state, proto, newState, registerFile, blk);
                newState->setPauseSource(gen->executionPauser());
                newState->pauseSource()->m_promiseCapability = PromiseObject::newPromiseCapability(*newState, newState->context()->globalObject()->promise());
                ExecutionPauser::start(state, newState->pauseSource(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::AsyncGenerator);
                generatorObject = gen;
            }

            return generatorObject;
        }


        ExecutionState* newState;

        if (std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value) {
            newState = new ExecutionState(ctx, nullptr, lexEnv, argc, argv, isStrict, ExecutionState::ForPauser);
            newState->setPauseSource(new ExecutionPauser(state, self, newState, registerFile, blk));
            newState->pauseSource()->m_promiseCapability = PromiseObject::newPromiseCapability(*newState, newState->context()->globalObject()->promise());
        } else {
            newState = new (alloca(sizeof(ExecutionState))) ExecutionState(ctx, &state, lexEnv, argc, argv, isStrict);
        }

        // prepare receiver(this variable)
        // we should use newState because
        // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
        // NOTE ToObject produces wrapper objects using calleeRealm. <<----
        stackStorage[0] = thisValueBinder(state, *newState, self, thisArgument, isStrict);

        if (isConstructCall) {
            NewTargetBinder newTargetBinder;
            newTargetBinder(state, *newState, self, newTarget, record);
        }

        // run function
        ReturnValueBinder returnValueBinder;
        const Value returnValue = returnValueBinder(state, *newState, self,
                                                    std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value ? ExecutionPauser::start(state, newState->pauseSource(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::Async)
                                                                                                                       : ByteCodeInterpreter::interpret(newState, blk, reinterpret_cast<const size_t>(blk->m_code.data()), registerFile),
                                                    thisArgument, record);

        if (UNLIKELY(blk->m_shouldClearStack)) {
            clearStack<512>();
        }

        return returnValue;
    }

private:
    template <typename FunctionObjectType, bool hasNewTargetOnEnvironment, bool canBindThisValueOnEnvironment>
    static NEVER_INLINE FunctionEnvironmentRecord* createFunctionEnvironmentRecord(ExecutionState& state, FunctionObjectType* self, InterpretedCodeBlock* codeBlock)
    {
        if (LIKELY(codeBlock->canUseIndexedVariableStorage())) {
            switch (codeBlock->identifierOnHeapCount()) {
            case 1:
                return new FunctionEnvironmentRecordOnHeapWithInlineStorage<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment, 1>(self);
            case 2:
                return new FunctionEnvironmentRecordOnHeapWithInlineStorage<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment, 2>(self);
            case 3:
                return new FunctionEnvironmentRecordOnHeapWithInlineStorage<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment, 3>(self);
            case 4:
                return new FunctionEnvironmentRecordOnHeapWithInlineStorage<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment, 4>(self);
            case 5:
                return new FunctionEnvironmentRecordOnHeapWithInlineStorage<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment, 5>(self);
            default:
                return new FunctionEnvironmentRecordOnHeap<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
            }
        } else {
            if (LIKELY(!codeBlock->needsVirtualIDOperation())) {
                return new FunctionEnvironmentRecordNotIndexed<canBindThisValueOnEnvironment, hasNewTargetOnEnvironment>(self);
            } else {
                return new FunctionEnvironmentRecordNotIndexedWithVirtualID(self);
            }
        }
    }
};

template <bool isConstruct, bool shouldReturnsObjectOnConstructCall>
Value NativeFunctionObject::processNativeFunctionCall(ExecutionState& state, const Value& receiverSrc, const size_t argc, Value* argv, Optional<Object*> newTarget)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY(state.stackLimit() > currentStackBase)) {
#else
    if (UNLIKELY(state.stackLimit() < currentStackBase)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Maximum call stack size exceeded");
    }

    NativeCodeBlock* codeBlock = nativeCodeBlock();
    Context* ctx = codeBlock->context();
    bool isStrict = codeBlock->isStrict();

    NativeFunctionPointer nativeFunc = codeBlock->nativeFunction();

    size_t len = codeBlock->functionLength();
    if (argc < len) {
        Value* newArgv = (Value*)alloca(sizeof(Value) * len);
        for (size_t i = 0; i < argc; i++) {
            newArgv[i] = argv[i];
        }
        for (size_t i = argc; i < len; i++) {
            newArgv[i] = Value();
        }
        argv = newArgv;
    }

    Value receiver = receiverSrc;
    ExecutionState newState(ctx, &state, this, argc, argv, isStrict);

    if (!isConstruct) {
        // prepare receiver
        if (UNLIKELY(!isStrict)) {
            if (receiver.isUndefinedOrNull()) {
                receiver = ctx->globalObject();
            } else {
                receiver = receiver.toObject(newState);
            }
        }
    }

    Value result;
    if (isConstruct) {
        result = nativeFunc(newState, receiver, argc, argv, newTarget);
        if (shouldReturnsObjectOnConstructCall && UNLIKELY(!result.isObject())) {
            ErrorObject::throwBuiltinError(newState, ErrorCode::TypeError, "Native Constructor must returns constructed new object");
        }
    } else {
        ASSERT(!newTarget);
        result = nativeFunc(newState, receiver, argc, argv, nullptr);
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state.context()->debugger(), &newState, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */
    return result;
}
} // namespace Escargot

#endif
