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
#include "runtime/AsyncGeneratorObject.h"
#include "runtime/ExecutionPauser.h"
#include "util/Util.h"

namespace Escargot {

class ScriptGeneratorFunctionObject;
class ScriptAsyncFunctionObject;

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
private:
    // https://www.ecma-international.org/ecma-262/10.0/#sec-getprototypefromconstructor
    static Value getPrototypeFromConstructor(ExecutionState& state, FunctionObject* constructor, const Value& intrinsicDefaultProto)
    {
        // Assert: intrinsicDefaultProto is a String value that is this specification's name of an intrinsic object. The corresponding object must be an intrinsic that is intended to be used as the [[Prototype]] value of an object.
        // Assert: IsCallable(constructor) is true.
        // Let proto be ? Get(constructor, "prototype").
        Value proto = constructor->getFunctionPrototype(state);
        // If Type(proto) is not Object, then
        if (!proto.isObject()) {
            // Let realm be ? GetFunctionRealm(constructor).
            // Set proto to realm's intrinsic object named intrinsicDefaultProto.
            proto = intrinsicDefaultProto;
        }
        // Return proto.
        return proto;
    }

public:
    template <typename FunctionObjectType, bool isConstructCall, bool hasNewTargetOnEnvironment, bool canBindThisValueOnEnvironment, typename ThisValueBinder, typename NewTargetBinder, typename ReturnValueBinder>
    static ALWAYS_INLINE Value processCall(ExecutionState& state, FunctionObjectType* self, const Value& thisArgument, const size_t argc, Value* argv, Object* newTarget) // newTarget is null on [[call]]
    {
        volatile int sp;
        size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
        if (UNLIKELY(state.stackLimit() > currentStackBase)) {
#else
        if (UNLIKELY(state.stackLimit() < currentStackBase)) {
#endif
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
        }

        ASSERT(self->codeBlock()->isInterpretedCodeBlock());

        CodeBlock* codeBlock = self->codeBlock();

        // prepare ByteCodeBlock if needed
        if (UNLIKELY(codeBlock->asInterpretedCodeBlock()->byteCodeBlock() == nullptr)) {
            self->generateByteCodeBlock(state);
        }

        ByteCodeBlock* blk = codeBlock->asInterpretedCodeBlock()->byteCodeBlock();
        Context* ctx = codeBlock->context();
        bool isStrict = codeBlock->isStrict();
        size_t registerSize = blk->m_requiredRegisterFileSizeInValueSize;
        size_t identifierOnStackCount = codeBlock->asInterpretedCodeBlock()->identifierOnStackCount();
        size_t stackStorageSize = codeBlock->asInterpretedCodeBlock()->totalStackAllocatedVariableSize();
        size_t literalStorageSize = blk->m_numeralLiteralData.size();
        Value* literalStorageSrc = blk->m_numeralLiteralData.data();

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

        if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncGeneratorFunctionObject>::value) {
            registerFile = (Value*)CustomAllocator<Value>().allocate(registerSize + stackStorageSize + literalStorageSize);
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

        // initialize identifiers by undefined value
        for (size_t i = 2; i < identifierOnStackCount; i++) {
            stackStorage[i] = Value();
        }

        ThisValueBinder thisValueBinder;
        if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value || std::is_same<FunctionObjectType, ScriptAsyncGeneratorFunctionObject>::value) {
            Value* arguments = CustomAllocator<Value>().allocate(argc);
            memcpy(arguments, argv, sizeof(Value) * argc);

            ExecutionState* newState = new ExecutionState(ctx, nullptr, lexEnv, argc, arguments, isStrict, ExecutionState::ForPauser);

            // prepare receiver(this variable)
            // we should use newState because
            // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
            // NOTE ToObject produces wrapper objects using calleeRealm. <<----
            stackStorage[0] = thisValueBinder(*newState, self, thisArgument, isStrict);

            if (isConstructCall) {
                NewTargetBinder newTargetBinder;
                newTargetBinder(*newState, self, newTarget, record);
            }

            // https://www.ecma-international.org/ecma-262/10.0/#sec-ordinarycreatefromconstructor
            // OrdinaryCreateFromConstructor ( constructor, intrinsicDefaultProto [ , internalSlotsList ] )
            FunctionObject* constructor = self;

            // Assert: intrinsicDefaultProto is a String value that is this specification's name of an intrinsic object. The corresponding object must be an intrinsic that is intended to be used as the [[Prototype]] value of an object.
            // let proto be ? GetPrototypeFromConstructor(constructor, intrinsicDefaultProto).
            Value proto(Value::ForceUninitialized);
            if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value) {
                proto = getPrototypeFromConstructor(state, constructor, constructor->getFunctionRealm(state)->globalObject()->generatorPrototype());
            } else {
                proto = getPrototypeFromConstructor(state, constructor, constructor->getFunctionRealm(state)->globalObject()->asyncGeneratorPrototype());
            }

            // Return ObjectCreate(proto, internalSlotsList).
            if (std::is_same<FunctionObjectType, ScriptGeneratorFunctionObject>::value) {
                GeneratorObject* gen = new GeneratorObject(state, newState, registerFile, blk, proto);
                newState->setPauseSource(gen->executionPauser());
                return gen;
            } else {
                AsyncGeneratorObject* gen = new AsyncGeneratorObject(state, newState, registerFile, blk, proto);
                newState->setPauseSource(gen->executionPauser());
                ExecutionPauser::start(state, newState->pauseSource(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::AsyncGenerator);
                return gen;
            }
        }


        ExecutionState* newState;

        if (std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value) {
            newState = new ExecutionState(ctx, nullptr, lexEnv, argc, argv, isStrict, ExecutionState::ForPauser);
            newState->setPauseSource(new ExecutionPauser(state, self, newState, registerFile, blk));
        } else {
            newState = new (alloca(sizeof(ExecutionState))) ExecutionState(ctx, &state, lexEnv, argc, argv, isStrict);
        }

        // prepare receiver(this variable)
        // we should use newState because
        // https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycallbindthis
        // NOTE ToObject produces wrapper objects using calleeRealm. <<----
        stackStorage[0] = thisValueBinder(*newState, self, thisArgument, isStrict);

        if (isConstructCall) {
            NewTargetBinder newTargetBinder;
            newTargetBinder(*newState, self, newTarget, record);
        }

        // run function
        ReturnValueBinder returnValueBinder;
        const Value returnValue = returnValueBinder(*newState, self,
                                                    std::is_same<FunctionObjectType, ScriptAsyncFunctionObject>::value ? ExecutionPauser::start(state, newState->pauseSource(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::Async)
                                                                                                                       : ByteCodeInterpreter::interpret(newState, blk, 0, registerFile),
                                                    thisArgument, record);

        if (UNLIKELY(blk->m_shouldClearStack)) {
            clearStack<512>();
        }

        return returnValue;
    }
};
}

#endif
