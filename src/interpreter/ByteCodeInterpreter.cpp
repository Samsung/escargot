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

#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/FunctionObject.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"
#include "runtime/GlobalObject.h"
#include "runtime/StringObject.h"
#include "runtime/NumberObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "runtime/IteratorOperations.h"
#include "runtime/GeneratorObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ScriptArrowFunctionObject.h"
#include "runtime/ScriptClassConstructorFunctionObject.h"
#include "runtime/ScriptClassMethodFunctionObject.h"
#include "runtime/ScriptGeneratorFunctionObject.h"
#include "parser/ScriptParser.h"
#include "util/Util.h"
#include "../third_party/checked_arithmetic/CheckedArithmetic.h"
#include "runtime/ProxyObject.h"

namespace Escargot {

#define ADD_PROGRAM_COUNTER(CodeType) programCounter += sizeof(CodeType);

ALWAYS_INLINE size_t jumpTo(char* codeBuffer, const size_t jumpPosition)
{
    return (size_t)&codeBuffer[jumpPosition];
}

ALWAYS_INLINE size_t resolveProgramCounter(char* codeBuffer, const size_t programCounter)
{
    return programCounter - (size_t)codeBuffer;
}

class ExecutionStateProgramCounterBinder {
public:
    ExecutionStateProgramCounterBinder(ExecutionState& state, size_t* newAddress)
        : m_state(state)
    {
        m_oldAddress = state.m_programCounter;
        state.m_programCounter = newAddress;
    }

    ~ExecutionStateProgramCounterBinder()
    {
        m_state.m_programCounter = m_oldAddress;
    }

private:
    ExecutionState& m_state;
    size_t* m_oldAddress;
};

Value ByteCodeInterpreter::interpret(ExecutionState* state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile)
{
#if defined(COMPILER_GCC)
    if (UNLIKELY(byteCodeBlock == nullptr)) {
        goto FillOpcodeTableLbl;
    }
#endif

    ASSERT(byteCodeBlock != nullptr);
    ASSERT(registerFile != nullptr);

    {
        ExecutionStateProgramCounterBinder binder(*state, &programCounter);
        char* codeBuffer = byteCodeBlock->m_code.data();
        programCounter = (size_t)(codeBuffer + programCounter);

#if defined(COMPILER_GCC)

#define DEFINE_OPCODE(codeName) codeName##OpcodeLbl
#define DEFINE_DEFAULT
#define NEXT_INSTRUCTION() \
    goto*(((ByteCode*)programCounter)->m_opcodeInAddress);
#define JUMP_INSTRUCTION(opcode) \
    goto opcode##OpcodeLbl;

        /* Execute first instruction. */
        NEXT_INSTRUCTION();
#else

#define DEFINE_OPCODE(codeName) case codeName##Opcode
#define DEFINE_DEFAULT                \
    default:                          \
        RELEASE_ASSERT_NOT_REACHED(); \
        }
#define NEXT_INSTRUCTION() \
    goto NextInstruction;
#define JUMP_INSTRUCTION(opcode)    \
    currentOpcode = opcode##Opcode; \
    goto NextInstructionWithoutFetchOpcode;

    NextInstruction:
        Opcode currentOpcode = ((ByteCode*)programCounter)->m_opcode;

    NextInstructionWithoutFetchOpcode:
        switch (currentOpcode) {
#endif

        DEFINE_OPCODE(LoadLiteral)
            :
        {
            LoadLiteral* code = (LoadLiteral*)programCounter;
            registerFile[code->m_registerIndex] = code->m_value;
            ADD_PROGRAM_COUNTER(LoadLiteral);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Move)
            :
        {
            Move* code = (Move*)programCounter;
            ASSERT(code->m_registerIndex1 < (byteCodeBlock->m_requiredRegisterFileSizeInValueSize + byteCodeBlock->m_codeBlock->asInterpretedCodeBlock()->totalStackAllocatedVariableSize() + 1));
            ASSERT(!registerFile[code->m_registerIndex0].isEmpty());
            registerFile[code->m_registerIndex1] = registerFile[code->m_registerIndex0];
            ADD_PROGRAM_COUNTER(Move);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetGlobalVariable)
            :
        {
            GetGlobalVariable* code = (GetGlobalVariable*)programCounter;
            ASSERT(byteCodeBlock->m_codeBlock->context() == state->context());
            Context* ctx = state->context();
            GlobalObject* globalObject = state->context()->globalObject();
            auto slot = code->m_slot;
            auto idx = slot->m_lexicalIndexCache;
            bool isCacheWork = false;

            if (LIKELY(idx != std::numeric_limits<size_t>::max())) {
                if (LIKELY(ctx->globalDeclarativeStorage().size() == slot->m_lexicalIndexCache && globalObject->structure() == slot->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= slot->m_cachedAddress);
                    ASSERT(slot->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    registerFile[code->m_registerIndex] = *((SmallValue*)slot->m_cachedAddress);
                    isCacheWork = true;
                } else if (slot->m_cachedStructure == nullptr) {
                    const SmallValue& val = ctx->globalDeclarativeStorage()[idx];
                    isCacheWork = true;
                    if (UNLIKELY(val.isEmpty())) {
                        ErrorObject::throwBuiltinError(*state, ErrorObject::ReferenceError, ctx->globalDeclarativeRecord()[idx].m_name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
                    }
                    registerFile[code->m_registerIndex] = val;
                }
            }
            if (UNLIKELY(!isCacheWork)) {
                registerFile[code->m_registerIndex] = getGlobalVariableSlowCase(*state, globalObject, slot, byteCodeBlock);
            }
            ADD_PROGRAM_COUNTER(GetGlobalVariable);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetGlobalVariable)
            :
        {
            SetGlobalVariable* code = (SetGlobalVariable*)programCounter;
            ASSERT(byteCodeBlock->m_codeBlock->context() == state->context());
            Context* ctx = state->context();
            GlobalObject* globalObject = state->context()->globalObject();
            auto slot = code->m_slot;
            auto idx = slot->m_lexicalIndexCache;

            bool isCacheWork = false;
            if (LIKELY(idx != std::numeric_limits<size_t>::max())) {
                if (LIKELY(ctx->globalDeclarativeStorage().size() == slot->m_lexicalIndexCache && globalObject->structure() == slot->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= slot->m_cachedAddress);
                    ASSERT(slot->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    *((SmallValue*)slot->m_cachedAddress) = registerFile[code->m_registerIndex];
                    isCacheWork = true;
                } else if (slot->m_cachedStructure == nullptr) {
                    isCacheWork = true;
                    if (UNLIKELY(ctx->globalDeclarativeStorage()[idx].isEmpty())) {
                        ErrorObject::throwBuiltinError(*state, ErrorObject::ReferenceError, ctx->globalDeclarativeRecord()[idx].m_name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
                    }
                    ctx->globalDeclarativeStorage()[idx] = registerFile[code->m_registerIndex];
                }
            }

            if (UNLIKELY(!isCacheWork)) {
                setGlobalVariableSlowCase(*state, globalObject, slot, registerFile[code->m_registerIndex], byteCodeBlock);
            }

            ADD_PROGRAM_COUNTER(SetGlobalVariable);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryPlus)
            :
        {
            BinaryPlus* code = (BinaryPlus*)programCounter;
            const Value& v0 = registerFile[code->m_srcIndex0];
            const Value& v1 = registerFile[code->m_srcIndex1];
            Value ret(Value::ForceUninitialized);
            if (v0.isInt32() && v1.isInt32()) {
                int32_t a = v0.asInt32();
                int32_t b = v1.asInt32();
                int32_t c;
                bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::add(a, b, c);
                if (LIKELY(result)) {
                    ret = Value(c);
                } else {
                    ret = Value(Value::EncodeAsDouble, (double)a + (double)b);
                }
            } else if (v0.isNumber() && v1.isNumber()) {
                ret = Value(v0.asNumber() + v1.asNumber());
            } else {
                ret = plusSlowCase(*state, v0, v1);
            }
            registerFile[code->m_dstIndex] = ret;
            ADD_PROGRAM_COUNTER(BinaryPlus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMinus)
            :
        {
            BinaryMinus* code = (BinaryMinus*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            Value ret(Value::ForceUninitialized);
            if (left.isInt32() && right.isInt32()) {
                int32_t a = left.asInt32();
                int32_t b = right.asInt32();
                int32_t c;
                bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::sub(a, b, c);
                if (LIKELY(result)) {
                    ret = Value(c);
                } else {
                    ret = Value(Value::EncodeAsDouble, (double)a - (double)b);
                }
            } else {
                ret = Value(left.toNumber(*state) - right.toNumber(*state));
            }
            registerFile[code->m_dstIndex] = ret;
            ADD_PROGRAM_COUNTER(BinaryMinus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMultiply)
            :
        {
            BinaryMultiply* code = (BinaryMultiply*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            Value ret(Value::ForceUninitialized);
            if (left.isInt32() && right.isInt32()) {
                int32_t a = left.asInt32();
                int32_t b = right.asInt32();
                if (UNLIKELY((!a || !b) && (a >> 31 || b >> 31))) { // -1 * 0 should be treated as -0, not +0
                    ret = Value(left.asNumber() * right.asNumber());
                } else {
                    int32_t c = right.asInt32();
                    bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::multiply(a, b, c);
                    if (LIKELY(result)) {
                        ret = Value(c);
                    } else {
                        ret = Value(Value::EncodeAsDouble, a * (double)b);
                    }
                }
            } else {
                auto first = left.toNumber(*state);
                auto second = right.toNumber(*state);
                ret = Value(Value::EncodeAsDouble, first * second);
            }
            registerFile[code->m_dstIndex] = ret;
            ADD_PROGRAM_COUNTER(BinaryMultiply);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryDivision)
            :
        {
            BinaryDivision* code = (BinaryDivision*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toNumber(*state) / right.toNumber(*state));
            ADD_PROGRAM_COUNTER(BinaryDivision);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryEqual)
            :
        {
            BinaryEqual* code = (BinaryEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.abstractEqualsTo(*state, right));
            ADD_PROGRAM_COUNTER(BinaryEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryNotEqual)
            :
        {
            BinaryNotEqual* code = (BinaryNotEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.abstractEqualsTo(*state, right));
            ADD_PROGRAM_COUNTER(BinaryNotEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryStrictEqual)
            :
        {
            BinaryStrictEqual* code = (BinaryStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.equalsTo(*state, right));
            ADD_PROGRAM_COUNTER(BinaryStrictEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryNotStrictEqual)
            :
        {
            BinaryNotStrictEqual* code = (BinaryNotStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.equalsTo(*state, right));
            ADD_PROGRAM_COUNTER(BinaryNotStrictEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLessThan)
            :
        {
            BinaryLessThan* code = (BinaryLessThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(*state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLessThanOrEqual)
            :
        {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(*state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThan)
            :
        {
            BinaryGreaterThan* code = (BinaryGreaterThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(*state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThanOrEqual)
            :
        {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(*state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ToNumberIncrement)
            :
        {
            ToNumberIncrement* code = (ToNumberIncrement*)programCounter;
            Value toNumberValue = Value(registerFile[code->m_srcIndex].toNumber(*state));
            registerFile[code->m_dstIndex] = toNumberValue;
            registerFile[code->m_storeIndex] = incrementOperation(*state, toNumberValue);
            ADD_PROGRAM_COUNTER(ToNumberIncrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Increment)
            :
        {
            Increment* code = (Increment*)programCounter;
            registerFile[code->m_dstIndex] = incrementOperation(*state, registerFile[code->m_srcIndex]);
            ADD_PROGRAM_COUNTER(Increment);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ToNumberDecrement)
            :
        {
            ToNumberDecrement* code = (ToNumberDecrement*)programCounter;
            Value toNumberValue = Value(registerFile[code->m_srcIndex].toNumber(*state));
            registerFile[code->m_dstIndex] = toNumberValue;
            registerFile[code->m_storeIndex] = decrementOperation(*state, toNumberValue);
            ADD_PROGRAM_COUNTER(ToNumberDecrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Decrement)
            :
        {
            Decrement* code = (Decrement*)programCounter;
            registerFile[code->m_dstIndex] = decrementOperation(*state, registerFile[code->m_srcIndex]);
            ADD_PROGRAM_COUNTER(Decrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryMinus)
            :
        {
            UnaryMinus* code = (UnaryMinus*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(-val.toNumber(*state));
            ADD_PROGRAM_COUNTER(UnaryMinus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryNot)
            :
        {
            UnaryNot* code = (UnaryNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(!val.toBoolean(*state));
            ADD_PROGRAM_COUNTER(UnaryNot);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetObject)
            :
        {
            GetObject* code = (GetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            PointerValue* v;
            if (LIKELY(willBeObject.isObject() && (v = willBeObject.asPointerValue())->hasTag(g_arrayObjectTag))) {
                ArrayObject* arr = (ArrayObject*)v;
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx = property.tryToUseAsArrayIndex(*state);
                    if (LIKELY(idx != Value::InvalidArrayIndexValue) && LIKELY(idx < arr->getArrayLength(*state))) {
                        const Value& v = arr->m_fastModeData[idx];
                        if (LIKELY(!v.isEmpty())) {
                            registerFile[code->m_storeRegisterIndex] = v;
                            ADD_PROGRAM_COUNTER(GetObject);
                            NEXT_INSTRUCTION();
                        }
                    }
                }
            }
            JUMP_INSTRUCTION(GetObjectOpcodeSlowCase);
        }

        DEFINE_OPCODE(SetObjectOperation)
            :
        {
            SetObjectOperation* code = (SetObjectOperation*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            if (LIKELY(willBeObject.isObject() && (willBeObject.asPointerValue())->hasTag(g_arrayObjectTag))) {
                ArrayObject* arr = willBeObject.asObject()->asArrayObject();
                uint32_t idx = property.tryToUseAsArrayIndex(*state);
                if (LIKELY(arr->isFastModeArray())) {
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        uint32_t len = arr->getArrayLength(*state);
                        if (UNLIKELY(len <= idx)) {
                            if (UNLIKELY(!arr->isExtensible(*state))) {
                                JUMP_INSTRUCTION(SetObjectOpcodeSlowCase);
                            }
                            if (UNLIKELY(!arr->setArrayLength(*state, idx + 1)) || UNLIKELY(!arr->isFastModeArray())) {
                                JUMP_INSTRUCTION(SetObjectOpcodeSlowCase);
                            }
                        }
                        arr->m_fastModeData[idx] = registerFile[code->m_loadRegisterIndex];
                        ADD_PROGRAM_COUNTER(SetObjectOperation);
                        NEXT_INSTRUCTION();
                    }
                }
            }
            JUMP_INSTRUCTION(SetObjectOpcodeSlowCase);
        }

        DEFINE_OPCODE(GetObjectPreComputedCase)
            :
        {
            GetObjectPreComputedCase* code = (GetObjectPreComputedCase*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            Object* obj;
            if (LIKELY(willBeObject.isObject())) {
                obj = willBeObject.asObject();
            } else {
                obj = fastToObject(*state, willBeObject);
            }
            registerFile[code->m_storeRegisterIndex] = getObjectPrecomputedCaseOperation(*state, obj, willBeObject, code->m_propertyName, code->m_inlineCache, byteCodeBlock);
            ADD_PROGRAM_COUNTER(GetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetObjectPreComputedCase)
            :
        {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)programCounter;
            setObjectPreComputedCaseOperation(*state, registerFile[code->m_objectRegisterIndex], code->m_propertyName, registerFile[code->m_loadRegisterIndex], *code->m_inlineCache, byteCodeBlock);
            ADD_PROGRAM_COUNTER(SetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Jump)
            :
        {
            Jump* code = (Jump*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            programCounter = code->m_jumpPosition;
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpIfRelation)
            :
        {
            JumpIfRelation* code = (JumpIfRelation*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            const Value& left = registerFile[code->m_registerIndex0];
            const Value& right = registerFile[code->m_registerIndex1];
            bool relation;
            if (code->m_isEqual) {
                relation = abstractRelationalComparisonOrEqual(*state, left, right, code->m_isLeftFirst);
            } else {
                relation = abstractRelationalComparison(*state, left, right, code->m_isLeftFirst);
            }

            if (relation) {
                ADD_PROGRAM_COUNTER(JumpIfRelation);
            } else {
                programCounter = code->m_jumpPosition;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpIfEqual)
            :
        {
            JumpIfEqual* code = (JumpIfEqual*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            const Value& left = registerFile[code->m_registerIndex0];
            const Value& right = registerFile[code->m_registerIndex1];
            bool equality;
            if (code->m_isStrict) {
                equality = left.equalsTo(*state, right);
            } else {
                equality = left.abstractEqualsTo(*state, right);
            }

            if (equality ^ code->m_shouldNegate) {
                ADD_PROGRAM_COUNTER(JumpIfEqual);
            } else {
                programCounter = code->m_jumpPosition;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpIfTrue)
            :
        {
            JumpIfTrue* code = (JumpIfTrue*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (registerFile[code->m_registerIndex].toBoolean(*state)) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfTrue);
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpIfFalse)
            :
        {
            JumpIfFalse* code = (JumpIfFalse*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (!registerFile[code->m_registerIndex].toBoolean(*state)) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfFalse);
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallFunction)
            :
        {
            CallFunction* code = (CallFunction*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];

            // if PointerValue is not callable, PointerValue::call function throws builtin error
            // https://www.ecma-international.org/ecma-262/6.0/#sec-call
            // If IsCallable(F) is false, throw a TypeError exception.
            if (UNLIKELY(!callee.isPointerValue())) {
                ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, errorMessage_NOT_Callable);
            }
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(*state, Value(), code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);

            ADD_PROGRAM_COUNTER(CallFunction);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallFunctionWithReceiver)
            :
        {
            CallFunctionWithReceiver* code = (CallFunctionWithReceiver*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            const Value& receiver = registerFile[code->m_receiverIndex];

            // if PointerValue is not callable, PointerValue::call function throws builtin error
            // https://www.ecma-international.org/ecma-262/6.0/#sec-call
            // If IsCallable(F) is false, throw a TypeError exception.
            if (UNLIKELY(!callee.isPointerValue())) {
                ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, errorMessage_NOT_Callable);
            }
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(*state, receiver, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);

            ADD_PROGRAM_COUNTER(CallFunctionWithReceiver);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadByHeapIndex)
            :
        {
            LoadByHeapIndex* code = (LoadByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = state->lexicalEnvironment();
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            registerFile[code->m_registerIndex] = upperEnv->record()->asDeclarativeEnvironmentRecord()->getHeapValueByIndex(*state, code->m_index);
            ADD_PROGRAM_COUNTER(LoadByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(StoreByHeapIndex)
            :
        {
            StoreByHeapIndex* code = (StoreByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = state->lexicalEnvironment();
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            upperEnv->record()->setMutableBindingByIndex(*state, code->m_index, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMod)
            :
        {
            BinaryMod* code = (BinaryMod*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = modOperation(*state, left, right);
            ADD_PROGRAM_COUNTER(BinaryMod);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseAnd)
            :
        {
            BinaryBitwiseAnd* code = (BinaryBitwiseAnd*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(*state) & right.toInt32(*state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseAnd);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseOr)
            :
        {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(*state) | right.toInt32(*state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseOr);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseXor)
            :
        {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(*state) ^ right.toInt32(*state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseXor);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLeftShift)
            :
        {
            BinaryLeftShift* code = (BinaryLeftShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            int32_t lnum = left.toInt32(*state);
            int32_t rnum = right.toInt32(*state);
            lnum <<= ((unsigned int)rnum) & 0x1F;
            registerFile[code->m_dstIndex] = Value(lnum);
            ADD_PROGRAM_COUNTER(BinaryLeftShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinarySignedRightShift)
            :
        {
            BinarySignedRightShift* code = (BinarySignedRightShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            int32_t lnum = left.toInt32(*state);
            int32_t rnum = right.toInt32(*state);
            lnum >>= ((unsigned int)rnum) & 0x1F;
            registerFile[code->m_dstIndex] = Value(lnum);
            ADD_PROGRAM_COUNTER(BinarySignedRightShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryUnsignedRightShift)
            :
        {
            BinaryUnsignedRightShift* code = (BinaryUnsignedRightShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            uint32_t lnum = left.toUint32(*state);
            uint32_t rnum = right.toUint32(*state);
            lnum = (lnum) >> ((rnum)&0x1F);
            registerFile[code->m_dstIndex] = Value(lnum);
            ADD_PROGRAM_COUNTER(BinaryUnsignedRightShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryBitwiseNot)
            :
        {
            UnaryBitwiseNot* code = (UnaryBitwiseNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(~val.toInt32(*state));
            ADD_PROGRAM_COUNTER(UnaryBitwiseNot);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetParameter)
            :
        {
            GetParameter* code = (GetParameter*)programCounter;
            if (code->m_paramIndex < state->argc()) {
                registerFile[code->m_registerIndex] = state->argv()[code->m_paramIndex];
            } else {
                registerFile[code->m_registerIndex] = Value();
            }
            ADD_PROGRAM_COUNTER(GetParameter);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(End)
            :
        {
            End* code = (End*)programCounter;
            return registerFile[code->m_registerIndex];
        }

        DEFINE_OPCODE(ToNumber)
            :
        {
            ToNumber* code = (ToNumber*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(val.toNumber(*state));
            ADD_PROGRAM_COUNTER(ToNumber);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeGlobalVariable)
            :
        {
            InitializeGlobalVariable* code = (InitializeGlobalVariable*)programCounter;
            ASSERT(byteCodeBlock->m_codeBlock->context() == state->context());
            initializeGlobalVariable(*state, code, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeGlobalVariable);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeByHeapIndex)
            :
        {
            InitializeByHeapIndex* code = (InitializeByHeapIndex*)programCounter;
            state->lexicalEnvironment()->record()->initializeBindingByIndex(*state, code->m_index, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineOwnPropertyOperation)
            :
        {
            ObjectDefineOwnPropertyOperation* code = (ObjectDefineOwnPropertyOperation*)programCounter;
            objectDefineOwnPropertyOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineOwnPropertyWithNameOperation)
            :
        {
            ObjectDefineOwnPropertyWithNameOperation* code = (ObjectDefineOwnPropertyWithNameOperation*)programCounter;
            objectDefineOwnPropertyWithNameOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyWithNameOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ArrayDefineOwnPropertyOperation)
            :
        {
            ArrayDefineOwnPropertyOperation* code = (ArrayDefineOwnPropertyOperation*)programCounter;
            arrayDefineOwnPropertyOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ArrayDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ArrayDefineOwnPropertyBySpreadElementOperation)
            :
        {
            ArrayDefineOwnPropertyBySpreadElementOperation* code = (ArrayDefineOwnPropertyBySpreadElementOperation*)programCounter;
            arrayDefineOwnPropertyBySpreadElementOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ArrayDefineOwnPropertyBySpreadElementOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateSpreadArrayObject)
            :
        {
            CreateSpreadArrayObject* code = (CreateSpreadArrayObject*)programCounter;
            createSpreadArrayObject(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(CreateSpreadArrayObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(NewOperation)
            :
        {
            NewOperation* code = (NewOperation*)programCounter;
            registerFile[code->m_resultIndex] = Object::construct(*state, registerFile[code->m_calleeIndex], code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(NewOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryTypeof)
            :
        {
            UnaryTypeof* code = (UnaryTypeof*)programCounter;
            unaryTypeof(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(UnaryTypeof);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetObjectOpcodeSlowCase)
            :
        {
            GetObject* code = (GetObject*)programCounter;
            getObjectOpcodeSlowCase(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(GetObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetObjectOpcodeSlowCase)
            :
        {
            SetObjectOperation* code = (SetObjectOperation*)programCounter;
            setObjectOpcodeSlowCase(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(SetObjectOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadByName)
            :
        {
            LoadByName* code = (LoadByName*)programCounter;
            registerFile[code->m_registerIndex] = loadByName(*state, state->lexicalEnvironment(), code->m_name);
            ADD_PROGRAM_COUNTER(LoadByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(StoreByName)
            :
        {
            StoreByName* code = (StoreByName*)programCounter;
            storeByName(*state, state->lexicalEnvironment(), code->m_name, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeByName)
            :
        {
            InitializeByName* code = (InitializeByName*)programCounter;
            initializeByName(*state, state->lexicalEnvironment(), code->m_name, code->m_isLexicallyDeclaredName, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateObject)
            :
        {
            CreateObject* code = (CreateObject*)programCounter;
            registerFile[code->m_registerIndex] = new Object(*state);
            ADD_PROGRAM_COUNTER(CreateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateArray)
            :
        {
            CreateArray* code = (CreateArray*)programCounter;
            ArrayObject* arr = new ArrayObject(*state);
            arr->setArrayLength(*state, code->m_length);
            registerFile[code->m_registerIndex] = arr;
            ADD_PROGRAM_COUNTER(CreateArray);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateFunction)
            :
        {
            CreateFunction* code = (CreateFunction*)programCounter;
            createFunctionOperation(*state, code, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(CreateFunction);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateClass)
            :
        {
            CreateClass* code = (CreateClass*)programCounter;
            classOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(CreateClass);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SuperReference)
            :
        {
            SuperReference* code = (SuperReference*)programCounter;
            superOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(SuperReference);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SuperSetObjectOperation)
            :
        {
            SuperSetObjectOperation* code = (SuperSetObjectOperation*)programCounter;
            superSetObjectOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(SuperSetObjectOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SuperGetObjectOperation)
            :
        {
            SuperGetObjectOperation* code = (SuperGetObjectOperation*)programCounter;
            registerFile[code->m_storeRegisterIndex] = superGetObjectOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(SuperGetObjectOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallSuper)
            :
        {
            CallSuper* code = (CallSuper*)programCounter;
            callSuperOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(CallSuper);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateRestElement)
            :
        {
            CreateRestElement* code = (CreateRestElement*)programCounter;
            registerFile[code->m_registerIndex] = createRestElementOperation(*state, byteCodeBlock);
            ADD_PROGRAM_COUNTER(CreateRestElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadThisBinding)
            :
        {
            LoadThisBinding* code = (LoadThisBinding*)programCounter;
            EnvironmentRecord* envRec = state->getThisEnvironment();
            ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
            registerFile[code->m_dstIndex] = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(*state);
            ADD_PROGRAM_COUNTER(LoadThisBinding);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallEvalFunction)
            :
        {
            CallEvalFunction* code = (CallEvalFunction*)programCounter;
            evalOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(CallEvalFunction);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TryOperation)
            :
        {
            Value v = tryOperation(state, programCounter, byteCodeBlock, registerFile);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TryCatchFinallyWithBlockBodyEnd)
            :
        {
            (*(state->rareData()->m_controlFlowRecord))[state->rareData()->m_controlFlowRecord->size() - 1] = nullptr;
            return Value();
        }

        DEFINE_OPCODE(ThrowOperation)
            :
        {
            ThrowOperation* code = (ThrowOperation*)programCounter;
            state->context()->throwException(*state, registerFile[code->m_registerIndex]);
        }

        DEFINE_OPCODE(WithOperation)
            :
        {
            Value v = withOperation(state, programCounter, byteCodeBlock, registerFile);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpComplexCase)
            :
        {
            JumpComplexCase* code = (JumpComplexCase*)programCounter;
            state->ensureRareData()->m_controlFlowRecord->back() = code->m_controlFlowRecord->clone();
            return Value();
        }

        DEFINE_OPCODE(EnumerateObject)
            :
        {
            EnumerateObject* code = (EnumerateObject*)programCounter;
            auto data = executeEnumerateObject(*state, registerFile[code->m_objectRegisterIndex].toObject(*state));
            registerFile[code->m_dataRegisterIndex] = Value((PointerValue*)data);
            ADD_PROGRAM_COUNTER(EnumerateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CheckIfKeyIsLast)
            :
        {
            CheckIfKeyIsLast* code = (CheckIfKeyIsLast*)programCounter;
            checkIfKeyIsLast(*state, code, codeBuffer, programCounter, registerFile);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(EnumerateObjectKey)
            :
        {
            EnumerateObjectKey* code = (EnumerateObjectKey*)programCounter;
            EnumerateObjectData* data = (EnumerateObjectData*)registerFile[code->m_dataRegisterIndex].asPointerValue();
            data->m_idx++;
            registerFile[code->m_registerIndex] = Value(data->m_keys[data->m_idx - 1]).toString(*state);
            ADD_PROGRAM_COUNTER(EnumerateObjectKey);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetIterator)
            :
        {
            GetIterator* code = (GetIterator*)programCounter;
            ByteCodeInterpreter::getIteratorOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(GetIterator);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(IteratorStep)
            :
        {
            IteratorStep* code = (IteratorStep*)programCounter;
            iteratorStepOperation(*state, programCounter, registerFile, codeBuffer);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(IteratorClose)
            :
        {
            IteratorClose* code = (IteratorClose*)programCounter;
            iteratorCloseOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(IteratorClose);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadRegexp)
            :
        {
            LoadRegexp* code = (LoadRegexp*)programCounter;
            auto reg = new RegExpObject(*state, code->m_body, code->m_option);
            registerFile[code->m_registerIndex] = reg;
            ADD_PROGRAM_COUNTER(LoadRegexp);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryDelete)
            :
        {
            UnaryDelete* code = (UnaryDelete*)programCounter;
            deleteOperation(*state, state->lexicalEnvironment(), code, registerFile);
            ADD_PROGRAM_COUNTER(UnaryDelete);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TemplateOperation)
            :
        {
            TemplateOperation* code = (TemplateOperation*)programCounter;
            templateOperation(*state, state->lexicalEnvironment(), code, registerFile);
            ADD_PROGRAM_COUNTER(TemplateOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryInOperation)
            :
        {
            BinaryInOperation* code = (BinaryInOperation*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            bool result = binaryInOperation(*state, left, right);
            registerFile[code->m_dstIndex] = Value(result);
            ADD_PROGRAM_COUNTER(BinaryInOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryInstanceOfOperation)
            :
        {
            BinaryInstanceOfOperation* code = (BinaryInstanceOfOperation*)programCounter;
            instanceOfOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(BinaryInstanceOfOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(NewTargetOperation)
            :
        {
            NewTargetOperation* code = (NewTargetOperation*)programCounter;
            newTargetOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(NewTargetOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineGetterSetter)
            :
        {
            ObjectDefineGetterSetter* code = (ObjectDefineGetterSetter*)programCounter;
            defineObjectGetterSetter(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineGetterSetter);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallFunctionInWithScope)
            :
        {
            CallFunctionInWithScope* code = (CallFunctionInWithScope*)programCounter;
            callFunctionInWithScope(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(CallFunctionInWithScope);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ReturnFunctionSlowCase)
            :
        {
            ReturnFunctionSlowCase* code = (ReturnFunctionSlowCase*)programCounter;
            if (UNLIKELY(state->rareData() != nullptr) && state->rareData()->m_controlFlowRecord && state->rareData()->m_controlFlowRecord->size()) {
                state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, registerFile[code->m_registerIndex], state->rareData()->m_controlFlowRecord->size());
            }
            return Value();
        }

        DEFINE_OPCODE(ThrowStaticErrorOperation)
            :
        {
            ThrowStaticErrorOperation* code = (ThrowStaticErrorOperation*)programCounter;
            ErrorObject::throwBuiltinError(*state, (ErrorObject::Code)code->m_errorKind, code->m_errorMessage, code->m_templateDataString);
        }

        DEFINE_OPCODE(CallFunctionWithSpreadElement)
            :
        {
            CallFunctionWithSpreadElement* code = (CallFunctionWithSpreadElement*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            const Value& receiver = code->m_receiverIndex == REGISTER_LIMIT ? Value() : registerFile[code->m_receiverIndex];

            // if PointerValue is not callable, PointerValue::call function throws builtin error
            // https://www.ecma-international.org/ecma-262/6.0/#sec-call
            // If IsCallable(F) is false, throw a TypeError exception.
            if (UNLIKELY(!callee.isPointerValue())) {
                ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, errorMessage_NOT_Callable);
            }
            ValueVector spreadArgs;
            spreadFunctionArguments(*state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(*state, receiver, spreadArgs.size(), spreadArgs.data());

            ADD_PROGRAM_COUNTER(CallFunctionWithSpreadElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(NewOperationWithSpreadElement)
            :
        {
            NewOperationWithSpreadElement* code = (NewOperationWithSpreadElement*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            ValueVector spreadArgs;
            spreadFunctionArguments(*state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
            registerFile[code->m_resultIndex] = Object::construct(*state, registerFile[code->m_calleeIndex], spreadArgs.size(), spreadArgs.data());
            ADD_PROGRAM_COUNTER(NewOperationWithSpreadElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BindingRestElement)
            :
        {
            BindingRestElement* code = (BindingRestElement*)programCounter;

            ArrayObject* array = new ArrayObject(*state);
            const Value& iterator = registerFile[code->m_iterIndex];

            size_t i = 0;
            while (true) {
                Value next = iteratorStep(*state, iterator);
                if (next.isFalse()) {
                    break;
                }

                array->setIndexedProperty(*state, Value(i++), iteratorValue(*state, next));
            }

            registerFile[code->m_dstIndex] = array;

            ADD_PROGRAM_COUNTER(BindingRestElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GeneratorResume)
            :
        {
            Value v = ByteCodeInterpreter::generatorResumeOperation(state, programCounter, byteCodeBlock);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Yield)
            :
        {
            yieldOperation(*state, registerFile, programCounter, codeBuffer);
            ASSERT_NOT_REACHED();
        }

        DEFINE_OPCODE(YieldDelegate)
            :
        {
            Value result = yieldDelegateOperation(*state, registerFile, programCounter, codeBuffer);

            if (result.isEmpty()) {
                NEXT_INSTRUCTION();
            }

            return result;
        }

        DEFINE_OPCODE(BlockOperation)
            :
        {
            BlockOperation* code = (BlockOperation*)programCounter;
            Value v = blockOperation(state, code, programCounter, byteCodeBlock, registerFile);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ReplaceBlockLexicalEnvironmentOperation)
            :
        {
            replaceBlockLexicalEnvironmentOperation(*state, programCounter, byteCodeBlock);
            ADD_PROGRAM_COUNTER(ReplaceBlockLexicalEnvironmentOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(EnsureArgumentsObject)
            :
        {
            ensureArgumentsObjectOperation(*state, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(EnsureArgumentsObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ResolveNameAddress)
            :
        {
            ResolveNameAddress* code = (ResolveNameAddress*)programCounter;
            resolveNameAddress(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(ResolveNameAddress);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(StoreByNameWithAddress)
            :
        {
            StoreByNameWithAddress* code = (StoreByNameWithAddress*)programCounter;
            storeByNameWithAddress(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(StoreByNameWithAddress);
            NEXT_INSTRUCTION();
        }

        DEFINE_DEFAULT
    }

    ASSERT_NOT_REACHED();

#if defined(COMPILER_GCC)
FillOpcodeTableLbl:
#define REGISTER_TABLE(opcode, pushCount, popCount) g_opcodeTable.m_table[opcode##Opcode] = &&opcode##OpcodeLbl;
    FOR_EACH_BYTECODE_OP(REGISTER_TABLE);
#undef REGISTER_TABLE
#endif

    return Value();
}

NEVER_INLINE EnvironmentRecord* ByteCodeInterpreter::getBindedEnvironmentRecordByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, Value& bindedValue, bool throwException)
{
    while (env) {
        EnvironmentRecord::GetBindingValueResult result = env->record()->getBindingValue(state, name);
        if (result.m_hasBindingValue) {
            bindedValue = result.m_value;
            return env->record();
        }
        env = env->outerEnvironment();
    }

    if (throwException)
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);

    return NULL;
}

NEVER_INLINE Value ByteCodeInterpreter::loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool throwException)
{
    while (env) {
        EnvironmentRecord::GetBindingValueResult result = env->record()->getBindingValue(state, name);
        if (result.m_hasBindingValue) {
            return result.m_value;
        }
        env = env->outerEnvironment();
    }

    if (UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
        Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, name.string());
        if (!virtialIdResult.isEmpty())
            return virtialIdResult;
    }

    if (throwException)
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);

    return Value();
}

NEVER_INLINE void ByteCodeInterpreter::storeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, const Value& value)
{
    while (env) {
        auto result = env->record()->hasBinding(state, name);
        if (result.m_index != SIZE_MAX) {
            env->record()->setMutableBindingByBindingSlot(state, result, name, value);
            return;
        }
        env = env->outerEnvironment();
    }
    if (state.inStrictMode()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);
    }
    GlobalObject* o = state.context()->globalObject();
    o->setThrowsExceptionWhenStrictMode(state, name, value, o);
}

NEVER_INLINE void ByteCodeInterpreter::initializeByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name, bool isLexicallyDeclaredName, const Value& value)
{
    if (isLexicallyDeclaredName) {
        state.lexicalEnvironment()->record()->initializeBinding(state, name, value);
    } else {
        while (env) {
            if (env->record()->isVarDeclarationTarget()) {
                auto result = env->record()->hasBinding(state, name);
                if (result.m_index != SIZE_MAX) {
                    env->record()->initializeBinding(state, name, value);
                    return;
                }
            }
            env = env->outerEnvironment();
        }
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);
    }
}

NEVER_INLINE void ByteCodeInterpreter::resolveNameAddress(ExecutionState& state, ResolveNameAddress* code, Value* registerFile)
{
    LexicalEnvironment* env = state.lexicalEnvironment();
    int64_t count = 0;
    while (env) {
        auto result = env->record()->hasBinding(state, code->m_name);
        if (result.m_index != SIZE_MAX) {
            registerFile[code->m_registerIndex] = Value(count);
            return;
        }
        count++;
        env = env->outerEnvironment();
    }
    registerFile[code->m_registerIndex] = Value(-1);
}

NEVER_INLINE void ByteCodeInterpreter::storeByNameWithAddress(ExecutionState& state, StoreByNameWithAddress* code, Value* registerFile)
{
    LexicalEnvironment* env = state.lexicalEnvironment();
    const Value& value = registerFile[code->m_valueRegisterIndex];
    int64_t count = registerFile[code->m_addressRegisterIndex].toNumber(state);
    if (count != -1) {
        int64_t idx = 0;
        while (env) {
            if (idx == count) {
                env->record()->setMutableBinding(state, code->m_name, value);
                return;
            }
            idx++;
            env = env->outerEnvironment();
        }
    }
    if (state.inStrictMode()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, code->m_name.string(), false, String::emptyString, errorMessage_IsNotDefined);
    }
    GlobalObject* o = state.context()->globalObject();
    o->setThrowsExceptionWhenStrictMode(state, code->m_name, value, o);
}

NEVER_INLINE Value ByteCodeInterpreter::plusSlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);
    Value lval(Value::ForceUninitialized);
    Value rval(Value::ForceUninitialized);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.12.8
    // No hint is provided in the calls to ToPrimitive in steps 5 and 6.
    // All native ECMAScript objects except Date objects handle the absence of a hint as if the hint Number were given;
    // Date objects handle the absence of a hint as if the hint String were given.
    // Host objects may handle the absence of a hint in some other manner.
    lval = left.toPrimitive(state);
    rval = right.toPrimitive(state);
    if (lval.isString() || rval.isString()) {
        ret = RopeString::createRopeString(lval.toString(state), rval.toString(state), &state);
    } else {
        ret = Value(lval.toNumber(state) + rval.toNumber(state));
    }

    return ret;
}

NEVER_INLINE Value ByteCodeInterpreter::modOperation(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);

    int32_t intLeft;
    int32_t intRight;
    if (left.isInt32() && ((intLeft = left.asInt32()) > 0) && right.isInt32() && (intRight = right.asInt32())) {
        ret = Value(intLeft % intRight);
    } else {
        double lvalue = left.toNumber(state);
        double rvalue = right.toNumber(state);
        // http://www.ecma-international.org/ecma-262/5.1/#sec-11.5.3
        if (std::isnan(lvalue) || std::isnan(rvalue)) {
            ret = Value(std::numeric_limits<double>::quiet_NaN());
        } else if (std::isinf(lvalue) || rvalue == 0 || rvalue == -0.0) {
            ret = Value(std::numeric_limits<double>::quiet_NaN());
        } else if (std::isinf(rvalue)) {
            ret = Value(lvalue);
        } else if (lvalue == 0.0) {
            if (std::signbit(lvalue))
                ret = Value(Value::EncodeAsDouble, -0.0);
            else
                ret = Value(0);
        } else {
            bool isLNeg = lvalue < 0.0;
            lvalue = std::abs(lvalue);
            rvalue = std::abs(rvalue);
            double r = fmod(lvalue, rvalue);
            if (isLNeg)
                r = -r;
            ret = Value(r);
        }
    }

    return ret;
}

NEVER_INLINE void ByteCodeInterpreter::instanceOfOperation(ExecutionState& state, BinaryInstanceOfOperation* code, Value* registerFile)
{
    registerFile[code->m_dstIndex] = Value(registerFile[code->m_srcIndex0].instanceOf(state, registerFile[code->m_srcIndex1]));
}

NEVER_INLINE void ByteCodeInterpreter::templateOperation(ExecutionState& state, LexicalEnvironment* env, TemplateOperation* code, Value* registerFile)
{
    const Value& s1 = registerFile[code->m_src0Index];
    const Value& s2 = registerFile[code->m_src1Index];

    StringBuilder builder;
    builder.appendString(s1.toString(state));
    builder.appendString(s2.toString(state));
    registerFile[code->m_dstIndex] = Value(builder.finalize(&state));
}

NEVER_INLINE void ByteCodeInterpreter::deleteOperation(ExecutionState& state, LexicalEnvironment* env, UnaryDelete* code, Value* registerFile)
{
    if (code->m_id.string()->length()) {
        bool result;
        AtomicString arguments = state.context()->staticStrings().arguments;
        if (UNLIKELY(code->m_id == arguments && !env->record()->isGlobalEnvironmentRecord())) {
            if (UNLIKELY(env->record()->isObjectEnvironmentRecord() && env->record()->hasBinding(state, arguments).m_index != SIZE_MAX)) {
                result = env->deleteBinding(state, code->m_id);
            } else {
                result = false;
            }
        } else {
            result = env->deleteBinding(state, code->m_id);
        }
        registerFile[code->m_dstIndex] = Value(result);
    } else {
        const Value& o = registerFile[code->m_srcIndex0];
        const Value& p = registerFile[code->m_srcIndex1];
        Object* obj = o.toObject(state);
        auto name = ObjectPropertyName(state, p);
        bool result = obj->deleteOwnProperty(state, name);
        if (!result && state.inStrictMode())
            Object::throwCannotDeleteError(state, name.toPropertyName(state));
        registerFile[code->m_dstIndex] = Value(result);
    }
}

ALWAYS_INLINE bool ByteCodeInterpreter::abstractRelationalComparison(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
{
    // consume very fast case
    if (LIKELY(left.isInt32() && right.isInt32())) {
        return left.asInt32() < right.asInt32();
    }

    if (LIKELY(left.isNumber() && right.isNumber())) {
        return left.asNumber() < right.asNumber();
    }

    return abstractRelationalComparisonSlowCase(state, left, right, leftFirst);
}

ALWAYS_INLINE bool ByteCodeInterpreter::abstractRelationalComparisonOrEqual(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
{
    // consume very fast case
    if (LIKELY(left.isInt32() && right.isInt32())) {
        return left.asInt32() <= right.asInt32();
    }

    if (LIKELY(left.isNumber() && right.isNumber())) {
        return left.asNumber() <= right.asNumber();
    }

    return abstractRelationalComparisonOrEqualSlowCase(state, left, right, leftFirst);
}

NEVER_INLINE bool ByteCodeInterpreter::abstractRelationalComparisonSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
{
    Value lval(Value::ForceUninitialized);
    Value rval(Value::ForceUninitialized);
    if (leftFirst) {
        lval = left.toPrimitive(state, Value::PreferNumber);
        rval = right.toPrimitive(state, Value::PreferNumber);
    } else {
        rval = right.toPrimitive(state, Value::PreferNumber);
        lval = left.toPrimitive(state, Value::PreferNumber);
    }

    if (lval.isInt32() && rval.isInt32()) {
        return lval.asInt32() < rval.asInt32();
    } else if (lval.isString() && rval.isString()) {
        return *lval.asString() < *rval.asString();
    } else {
        double n1 = lval.toNumber(state);
        double n2 = rval.toNumber(state);
        return n1 < n2;
    }
}

NEVER_INLINE bool ByteCodeInterpreter::abstractRelationalComparisonOrEqualSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
{
    Value lval(Value::ForceUninitialized);
    Value rval(Value::ForceUninitialized);
    if (leftFirst) {
        lval = left.toPrimitive(state, Value::PreferNumber);
        rval = right.toPrimitive(state, Value::PreferNumber);
    } else {
        rval = right.toPrimitive(state, Value::PreferNumber);
        lval = left.toPrimitive(state, Value::PreferNumber);
    }

    if (lval.isInt32() && rval.isInt32()) {
        return lval.asInt32() <= rval.asInt32();
    } else if (lval.isString() && rval.isString()) {
        return *lval.asString() <= *rval.asString();
    } else {
        double n1 = lval.toNumber(state);
        double n2 = rval.toNumber(state);
        return n1 <= n2;
    }
}

ALWAYS_INLINE Value ByteCodeInterpreter::getObjectPrecomputedCaseOperation(ExecutionState& state, Object* obj, const Value& receiver, const PropertyName& name, GetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    Object* orgObj = obj;
    const size_t cacheFillCount = inlineCache.m_cache.size();
    GetObjectInlineCacheData* cacheData = inlineCache.m_cache.data();
    unsigned currentCacheIndex = 0;
TestCache:
    for (; currentCacheIndex < cacheFillCount; currentCacheIndex++) {
        obj = orgObj;
        GetObjectInlineCacheData& data = cacheData[currentCacheIndex];
        const size_t cSiz = data.m_cachedhiddenClassChain.size() - 1;
        ObjectStructureChainItem* cachedHiddenClassChain = data.m_cachedhiddenClassChain.data();
        size_t cachedIndex = data.m_cachedIndex;
        size_t i;
        for (i = 0; i < cSiz; i++) {
            if (cachedHiddenClassChain[i].m_objectStructure != obj->structure()) {
                currentCacheIndex++;
                goto TestCache;
            }
            Object* protoObject = obj->Object::getPrototypeObject(state);
            if (protoObject != nullptr) {
                obj = protoObject;
            } else {
                currentCacheIndex++;
                goto TestCache;
            }
        }

        if (LIKELY(cachedHiddenClassChain[cSiz].m_objectStructure == obj->structure())) {
            if (LIKELY(data.m_cachedIndex != SIZE_MAX)) {
                return obj->getOwnPropertyUtilForObject(state, data.m_cachedIndex, receiver);
            } else {
                return Value();
            }
        }
    }

    return getObjectPrecomputedCaseOperationCacheMiss(state, orgObj, receiver, name, inlineCache, block);
}

NEVER_INLINE Value ByteCodeInterpreter::getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& receiver, const PropertyName& name, GetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    const int maxCacheMissCount = 16;
    const int minCacheFillCount = 3;
    const size_t maxCacheCount = 10;
    // cache miss.
    inlineCache.m_executeCount++;
    if (inlineCache.m_executeCount <= minCacheFillCount) {
        return obj->get(state, ObjectPropertyName(state, name)).value(state, receiver);
    }

    if (inlineCache.m_cache.size())
        inlineCache.m_cacheMissCount++;

    if (inlineCache.m_cache.size() > maxCacheCount) {
        return obj->get(state, ObjectPropertyName(state, name)).value(state, receiver);
    }

    if (UNLIKELY(!obj->isInlineCacheable())) {
        return obj->get(state, ObjectPropertyName(state, name)).value(state, receiver);
    }

    Object* orgObj = obj;
    inlineCache.m_cache.insert(inlineCache.m_cache.begin(), GetObjectInlineCacheData());

    ObjectStructureChain* cachedHiddenClassChain = &inlineCache.m_cache[0].m_cachedhiddenClassChain;

    ObjectStructureChainItem newItem;
    while (true) {
        newItem.m_objectStructure = obj->structure();

        cachedHiddenClassChain->push_back(newItem);
        size_t idx = obj->structure()->findProperty(state, name);

        if (!obj->structure()->isProtectedByTransitionTable()) {
            block->m_objectStructuresInUse->insert(obj->structure());
        }

        if (idx != SIZE_MAX) {
            inlineCache.m_cache[0].m_cachedIndex = idx;
            break;
        }
        obj = obj->Object::getPrototypeObject(state);
        if (!obj) {
            break;
        }
    }

    if (inlineCache.m_cache[0].m_cachedIndex != SIZE_MAX) {
        return obj->getOwnPropertyUtilForObject(state, inlineCache.m_cache[0].m_cachedIndex, receiver);
    } else {
        return Value();
    }
}

ALWAYS_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperation(ExecutionState& state, const Value& willBeObject, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    Object* obj;
    if (UNLIKELY(!willBeObject.isObject())) {
        obj = willBeObject.toObject(state);
        if (willBeObject.isPrimitive()) {
            obj->preventExtensions(state);
        }
    } else {
        obj = willBeObject.asObject();
    }
    Object* originalObject = obj;
    ASSERT(originalObject != nullptr);

    ObjectStructureChainItem testItem;
    testItem.m_objectStructure = obj->structure();

    if (inlineCache.m_cachedIndex != SIZE_MAX && inlineCache.m_cachedhiddenClassChain[0] == testItem) {
        ASSERT(inlineCache.m_cachedhiddenClassChain.size() == 1);
        // cache hit!
        obj->m_values[inlineCache.m_cachedIndex] = value;
        return;
    } else if (inlineCache.m_hiddenClassWillBe) {
        int cSiz = inlineCache.m_cachedhiddenClassChain.size();
        bool miss = false;
        for (int i = 0; i < cSiz - 1; i++) {
            testItem.m_objectStructure = obj->structure();
            if (UNLIKELY(inlineCache.m_cachedhiddenClassChain[i] != testItem)) {
                miss = true;
                break;
            } else {
                Object* o = obj->Object::getPrototypeObject(state);
                if (UNLIKELY(!o)) {
                    miss = true;
                    break;
                }
                obj = o;
            }
        }
        if (LIKELY(!miss) && inlineCache.m_cachedhiddenClassChain[cSiz - 1].m_objectStructure == obj->structure()) {
            // cache hit!
            obj = originalObject;
            ASSERT(!obj->structure()->isStructureWithFastAccess());
            obj->m_values.push_back(value, inlineCache.m_hiddenClassWillBe->propertyCount());
            obj->m_structure = inlineCache.m_hiddenClassWillBe;
            return;
        }
    }

    setObjectPreComputedCaseOperationCacheMiss(state, originalObject, willBeObject, name, value, inlineCache, block);
}

NEVER_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* originalObject, const Value& willBeObject, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    // cache miss
    if (inlineCache.m_cacheMissCount > 16) {
        inlineCache.invalidateCache();
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, willBeObject);
        return;
    }

    if (UNLIKELY(!originalObject->isInlineCacheable())) {
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, willBeObject);
        return;
    }

    inlineCache.invalidateCache();
    inlineCache.m_cacheMissCount++;

    Object* obj = originalObject;

    size_t idx = obj->structure()->findProperty(state, name);
    if (idx != SIZE_MAX) {
        // own property
        ObjectStructureChainItem newItem;
        newItem.m_objectStructure = obj->structure();

        obj->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value, willBeObject);
        // Don't update the inline cache if the property is removed by a setter function.
        /* example code
        var o = { set foo (a) { var a = delete o.foo } };
        o.foo = 0;
        */
        if (UNLIKELY(idx >= obj->structure()->propertyCount())) {
            return;
        }

        const auto& propertyData = obj->structure()->readProperty(state, idx);
        const auto& desc = propertyData.m_descriptor;
        if (propertyData.m_propertyName == name && desc.isPlainDataProperty() && desc.isWritable()) {
            inlineCache.m_cachedIndex = idx;
            inlineCache.m_cachedhiddenClassChain.push_back(newItem);
        }
    } else {
        Object* orgObject = obj;
        if (UNLIKELY(obj->structure()->isStructureWithFastAccess())) {
            inlineCache.invalidateCache();
            orgObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, willBeObject);
            return;
        }

        ObjectStructureChainItem newItem;
        newItem.m_objectStructure = obj->structure();
        inlineCache.m_cachedhiddenClassChain.push_back(newItem);
        Value proto = obj->getPrototype(state);
        while (proto.isObject()) {
            obj = proto.asObject();
            newItem.m_objectStructure = obj->structure();
            inlineCache.m_cachedhiddenClassChain.push_back(newItem);
            proto = obj->getPrototype(state);
        }
        bool s = orgObject->set(state, ObjectPropertyName(state, name), value, willBeObject);
        if (UNLIKELY(!s)) {
            if (state.inStrictMode())
                orgObject->throwCannotWriteError(state, name);

            inlineCache.invalidateCache();
            return;
        }
        if (orgObject->structure()->isStructureWithFastAccess()) {
            inlineCache.invalidateCache();
            return;
        }

        auto result = orgObject->get(state, ObjectPropertyName(state, name));
        if (!result.hasValue() || !result.isDataProperty()) {
            inlineCache.invalidateCache();
            return;
        }

        inlineCache.m_hiddenClassWillBe = orgObject->structure();
    }
}

NEVER_INLINE EnumerateObjectData* ByteCodeInterpreter::executeEnumerateObject(ExecutionState& state, Object* obj)
{
    EnumerateObjectData* data = new EnumerateObjectData();
    data->m_object = obj;
    data->m_originalLength = 0;
    if (obj->isArrayObject())
        data->m_originalLength = obj->length(state);
    Value target = data->m_object;
    bool shouldSearchProto = false;
    ObjectStructureChainItem newItem;
    newItem.m_objectStructure = target.asObject()->structure();

    data->m_hiddenClassChain.push_back(newItem);

    std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_allocator<String*>> keyStringSet;

    target = target.asObject()->getPrototype(state);
    while (target.isObject()) {
        if (!shouldSearchProto) {
            target.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
                if (desc.isEnumerable()) {
                    bool* shouldSearchProto = (bool*)data;
                    *shouldSearchProto = true;
                    return false;
                }
                return true;
            },
                                           &shouldSearchProto);
        }
        newItem.m_objectStructure = target.asObject()->structure();
        data->m_hiddenClassChain.push_back(newItem);
        target = target.asObject()->getPrototype(state);
    }

    target = obj;

    if (shouldSearchProto) {
        struct EData {
            std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_allocator<String*>>* keyStringSet;
            EnumerateObjectData* data;
            Object* obj;
        } eData;

        eData.data = data;
        eData.keyStringSet = &keyStringSet;
        eData.obj = obj;
        while (target.isObject()) {
            target.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
                EData* eData = (EData*)data;
                if (desc.isEnumerable()) {
                    String* key = name.toPlainValue(state).toString(state);
                    auto iter = eData->keyStringSet->find(key);
                    if (iter == eData->keyStringSet->end()) {
                        eData->keyStringSet->insert(key);
                        eData->data->m_keys.pushBack(name.toPlainValue(state));
                    }
                } else if (self == eData->obj) {
                    // 12.6.4 The values of [[Enumerable]] attributes are not considered
                    // when determining if a property of a prototype object is shadowed by a previous object on the prototype chain.
                    String* key = name.toPlainValue(state).toString(state);
                    ASSERT(eData->keyStringSet->find(key) == eData->keyStringSet->end());
                    eData->keyStringSet->insert(key);
                }
                return true;
            },
                                           &eData);
            target = target.asObject()->getPrototype(state);
        }
    } else {
        ValueVector keys = target.asObject()->enumerableOwnProperties(state, target.asObject(), EnumerableOwnPropertiesType::Key);
        size_t keysSize = keys.size();
        data->m_keys.resizeWithUninitializedValues(keysSize);
        for (uint32_t i = 0; i < keysSize; i++) {
            data->m_keys[i] = SmallValue(keys[i]);
        }
    }


    if (obj->rareData()) {
        obj->rareData()->m_shouldUpdateEnumerateObjectData = false;
    }
    return data;
}

NEVER_INLINE EnumerateObjectData* ByteCodeInterpreter::updateEnumerateObjectData(ExecutionState& state, EnumerateObjectData* data)
{
    EnumerateObjectData* newData = executeEnumerateObject(state, data->m_object);
    std::vector<Value, GCUtil::gc_malloc_allocator<Value>> oldKeys;
    if (data->m_keys.size()) {
        oldKeys.insert(oldKeys.end(), &data->m_keys[0], &data->m_keys[data->m_keys.size() - 1] + 1);
    }
    std::vector<Value, GCUtil::gc_malloc_allocator<Value>> differenceKeys;
    for (size_t i = 0; i < newData->m_keys.size(); i++) {
        const Value& key = newData->m_keys[i];
        // If a property that has not yet been visited during enumeration is deleted, then it will not be visited.
        if (std::find(oldKeys.begin(), oldKeys.begin() + data->m_idx, key) == oldKeys.begin() + data->m_idx && std::find(oldKeys.begin() + data->m_idx, oldKeys.end(), key) != oldKeys.end()) {
            // If new properties are added to the object being enumerated during enumeration,
            // the newly added properties are not guaranteed to be visited in the active enumeration.
            differenceKeys.push_back(key);
        }
    }
    data = newData;
    data->m_keys.clear();
    data->m_keys.resizeWithUninitializedValues(differenceKeys.size());
    for (size_t i = 0; i < differenceKeys.size(); i++) {
        data->m_keys[i] = differenceKeys[i];
    }
    return data;
}

ALWAYS_INLINE Object* ByteCodeInterpreter::fastToObject(ExecutionState& state, const Value& obj)
{
    if (LIKELY(obj.isString())) {
        StringObject* o = state.context()->globalObject()->stringProxyObject();
        o->setPrimitiveValue(state, obj.asString());
        return o;
    } else if (obj.isNumber()) {
        NumberObject* o = state.context()->globalObject()->numberProxyObject();
        o->setPrimitiveValue(state, obj.asNumber());
        return o;
    }
    return obj.toObject(state);
}

NEVER_INLINE Value ByteCodeInterpreter::getGlobalVariableSlowCase(ExecutionState& state, Object* go, GlobalVariableAccessCacheItem* slot, ByteCodeBlock* block)
{
    Context* ctx = state.context();
    auto& records = ctx->globalDeclarativeRecord();
    AtomicString name = slot->m_propertyName;
    auto siz = records.size();

    for (size_t i = 0; i < siz; i++) {
        if (records[i].m_name == name) {
            slot->m_lexicalIndexCache = i;
            slot->m_cachedAddress = nullptr;
            slot->m_cachedStructure = nullptr;
            auto v = state.context()->globalDeclarativeStorage()[i];
            if (UNLIKELY(v.isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
            }
            return v;
        }
    }

    size_t idx = go->structure()->findProperty(state, slot->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        ObjectGetResult res = go->get(state, ObjectPropertyName(state, slot->m_propertyName));
        if (res.hasValue()) {
            return res.value(state, go);
        } else {
            if (UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
                Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, name.string());
                if (!virtialIdResult.isEmpty())
                    return virtialIdResult;
            }
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);
            ASSERT_NOT_REACHED();
        }
    } else {
        const ObjectStructureItem& item = go->structure()->readProperty(state, idx);
        if (!item.m_descriptor.isPlainDataProperty() || !item.m_descriptor.isWritable()) {
            slot->m_cachedStructure = nullptr;
            slot->m_cachedAddress = nullptr;
            slot->m_lexicalIndexCache = std::numeric_limits<size_t>::max();
            return go->getOwnPropertyUtilForObject(state, idx, go);
        }

        slot->m_cachedAddress = &go->m_values.data()[idx];
        slot->m_cachedStructure = go->structure();
        slot->m_lexicalIndexCache = siz;
        return *((SmallValue*)slot->m_cachedAddress);
    }
}

class VirtualIdDisabler {
public:
    explicit VirtualIdDisabler(Context* c)
        : fn(c->virtualIdentifierCallback())
        , ctx(c)
    {
        c->setVirtualIdentifierCallback(nullptr);
    }
    ~VirtualIdDisabler()
    {
        ctx->setVirtualIdentifierCallback(fn);
    }

    VirtualIdentifierCallback fn;
    Context* ctx;
};

NEVER_INLINE void ByteCodeInterpreter::setGlobalVariableSlowCase(ExecutionState& state, Object* go, GlobalVariableAccessCacheItem* slot, const Value& value, ByteCodeBlock* block)
{
    Context* ctx = state.context();
    auto& records = ctx->globalDeclarativeRecord();
    AtomicString name = slot->m_propertyName;
    auto siz = records.size();
    for (size_t i = 0; i < siz; i++) {
        if (records[i].m_name == name) {
            slot->m_lexicalIndexCache = i;
            slot->m_cachedAddress = nullptr;
            slot->m_cachedStructure = nullptr;
            auto& place = ctx->globalDeclarativeStorage()[i];
            if (UNLIKELY(place.isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
            }
            place = value;
            return;
        }
    }


    size_t idx = go->structure()->findProperty(state, slot->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        if (UNLIKELY(state.inStrictMode())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, slot->m_propertyName.string(), false, String::emptyString, errorMessage_IsNotDefined);
        }
        VirtualIdDisabler d(state.context());
        go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, slot->m_propertyName), value, go);
    } else {
        const ObjectStructureItem& item = go->structure()->readProperty(state, idx);
        if (!item.m_descriptor.isPlainDataProperty() || !item.m_descriptor.isWritable()) {
            slot->m_cachedStructure = nullptr;
            slot->m_cachedAddress = nullptr;
            slot->m_lexicalIndexCache = std::numeric_limits<size_t>::max();
            go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, slot->m_propertyName), value, go);
            return;
        }

        slot->m_cachedAddress = &go->m_values.data()[idx];
        slot->m_cachedStructure = go->structure();
        slot->m_lexicalIndexCache = siz;

        go->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value, go);
    }
}

NEVER_INLINE void ByteCodeInterpreter::initializeGlobalVariable(ExecutionState& state, InitializeGlobalVariable* code, const Value& value)
{
    Context* ctx = state.context();
    auto& records = ctx->globalDeclarativeRecord();
    for (size_t i = 0; i < records.size(); i++) {
        if (records[i].m_name == code->m_variableName) {
            state.context()->globalDeclarativeStorage()[i] = value;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

NEVER_INLINE void ByteCodeInterpreter::createFunctionOperation(ExecutionState& state, CreateFunction* code, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    CodeBlock* cb = code->m_codeBlock;

    LexicalEnvironment* outerLexicalEnvironment = state.mostNearestHeapAllocatedLexicalEnvironment();

    if (cb->isGenerator()) {
        Value thisValue = cb->isArrowFunctionExpression() ? registerFile[byteCodeBlock->m_requiredRegisterFileSizeInValueSize] : Value(Value::EmptyValue);
        Object* homeObject = (cb->isClassMethod() || cb->isClassStaticMethod()) ? registerFile[code->m_homeObjectRegisterIndex].asObject() : nullptr;
        registerFile[code->m_registerIndex] = new ScriptGeneratorFunctionObject(state, code->m_codeBlock, outerLexicalEnvironment, thisValue, homeObject);
    } else if (cb->isArrowFunctionExpression()) {
        registerFile[code->m_registerIndex] = new ScriptArrowFunctionObject(state, code->m_codeBlock, outerLexicalEnvironment, registerFile[byteCodeBlock->m_requiredRegisterFileSizeInValueSize]);
    } else if (cb->isClassMethod() || cb->isClassStaticMethod()) {
        registerFile[code->m_registerIndex] = new ScriptClassMethodFunctionObject(state, code->m_codeBlock, outerLexicalEnvironment, registerFile[code->m_homeObjectRegisterIndex].asObject());
    } else {
        registerFile[code->m_registerIndex] = new ScriptFunctionObject(state, code->m_codeBlock, outerLexicalEnvironment, true, false);
    }
}

NEVER_INLINE ArrayObject* ByteCodeInterpreter::createRestElementOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock)
{
    ASSERT(state.resolveCallee());

    ArrayObject* newArray;
    size_t parameterLen = (size_t)byteCodeBlock->m_codeBlock->parameterCount();
    size_t argc = state.argc();
    Value* argv = state.argv();

    if (argc > parameterLen) {
        size_t arrLen = argc - parameterLen;
        newArray = new ArrayObject(state, (double)arrLen);
        for (size_t i = 0; i < arrLen; i++) {
            newArray->setIndexedProperty(state, Value(i), argv[parameterLen + i]);
        }
    } else {
        newArray = new ArrayObject(state);
    }

    return newArray;
}

NEVER_INLINE Value ByteCodeInterpreter::tryOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    char* codeBuffer = byteCodeBlock->m_code.data();
    TryOperation* code = (TryOperation*)programCounter;
    bool oldInTryStatement = state->m_inTryStatement;

    bool inGeneratorScope = state->inGeneratorScope();
    bool inGeneratorResumeProcess = code->m_isTryResumeProcess || code->m_isCatchResumeProcess || code->m_isFinallyResumeProcess;
    bool shouldUseHeapAllocatedState = inGeneratorScope && !inGeneratorResumeProcess;
    ExecutionState* newState;

    if (UNLIKELY(shouldUseHeapAllocatedState)) {
        newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
        newState->ensureRareData();
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
    }

    if (!LIKELY(inGeneratorResumeProcess)) {
        if (!state->ensureRareData()->m_controlFlowRecord) {
            state->ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state->ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    }

    if (LIKELY(!code->m_isCatchResumeProcess && !code->m_isFinallyResumeProcess)) {
        try {
            newState->m_inTryStatement = true;
            size_t newPc = programCounter + sizeof(TryOperation);
            clearStack<386>();
            interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);
            if (UNLIKELY(code->m_isTryResumeProcess)) {
                state = newState->parent();
                code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
                newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
                newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
            }
            newState->m_inTryStatement = oldInTryStatement;
        } catch (const Value& val) {
            if (UNLIKELY(code->m_isTryResumeProcess)) {
                state = newState->parent();
                code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
                newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
                newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
            }
            newState->m_inTryStatement = oldInTryStatement;
            newState->context()->m_sandBoxStack.back()->fillStackDataIntoErrorObject(val);

#ifndef NDEBUG
            if (getenv("DUMP_ERROR_IN_TRY_CATCH") && strlen(getenv("DUMP_ERROR_IN_TRY_CATCH"))) {
                ErrorObject::StackTraceData* data = ErrorObject::StackTraceData::create(newState->context()->m_sandBoxStack.back());
                StringBuilder builder;
                builder.appendString("Caught error in try-catch block\n");
                data->buildStackTrace(newState->context(), builder);
                ESCARGOT_LOG_ERROR("%s\n", builder.finalize()->toUTF8StringData().data());
            }
#endif

            newState->context()->m_sandBoxStack.back()->m_stackTraceData.clear();
            if (!code->m_hasCatch) {
                newState->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
            } else {
                registerFile[code->m_catchedValueRegisterIndex] = val;
                try {
                    interpret(newState, byteCodeBlock, code->m_catchPosition, registerFile);
                } catch (const Value& val) {
                    newState->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                }
            }
        }
    } else if (code->m_isCatchResumeProcess) {
        try {
            interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, programCounter + sizeof(TryOperation)), registerFile);
            state = newState->parent();
            code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
        } catch (const Value& val) {
            state = newState->parent();
            code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
            state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
        }
    }

    if (code->m_isFinallyResumeProcess) {
        interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, programCounter + sizeof(TryOperation)), registerFile);
        state = newState->parent();
        code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    } else if (code->m_hasFinalizer) {
        if (UNLIKELY(code->m_isTryResumeProcess || code->m_isCatchResumeProcess)) {
            state = newState->parent();
            code = (TryOperation*)(codeBuffer + newState->rareData()->m_programCounterWhenItStoppedByYield);
            newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
            newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
        }
        interpret(newState, byteCodeBlock, code->m_tryCatchEndPosition, registerFile);
    }

    ControlFlowRecord* record = state->rareData()->m_controlFlowRecord->back();
    state->rareData()->m_controlFlowRecord->erase(state->rareData()->m_controlFlowRecord->size() - 1);

    if (record != nullptr) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count() && (record->outerLimitCount() < record->count())) {
                state->rareData()->m_controlFlowRecord->back() = record;
                return Value();
            } else {
                programCounter = jumpTo(codeBuffer, pos);
                return Value(Value::EmptyValue);
            }
        } else if (record->reason() == ControlFlowRecord::NeedsThrow) {
            state->context()->throwException(*state, record->value());
            ASSERT_NOT_REACHED();
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state->rareData()->m_controlFlowRecord->back() = record;
            }
            return record->value();
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_finallyEndPosition);
        return Value(Value::EmptyValue);
    }
}

NEVER_INLINE void ByteCodeInterpreter::evalOperation(ExecutionState& state, CallEvalFunction* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    size_t argc;
    Value* argv;
    ValueVector spreadArgs;

    if (code->m_hasSpreadElement) {
        spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
        argv = spreadArgs.data();
        argc = spreadArgs.size();
    } else {
        argc = code->m_argumentCount;
        argv = &registerFile[code->m_argumentsStartIndex];
    }

    Value eval = registerFile[code->m_evalIndex];
    if (eval.equalsTo(state, state.context()->globalObject()->eval())) {
        // do eval
        Value arg;
        if (argc) {
            arg = argv[0];
        }
        Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
        registerFile[code->m_resultIndex] = state.context()->globalObject()->evalLocal(state, arg, stackStorage[0], byteCodeBlock->m_codeBlock->asInterpretedCodeBlock(), code->m_inWithScope);
    } else {
        Value thisValue;
        if (code->m_inWithScope) {
            LexicalEnvironment* env = state.lexicalEnvironment();
            while (env) {
                EnvironmentRecord::GetBindingValueResult result = env->record()->getBindingValue(state, state.context()->staticStrings().eval);
                if (result.m_hasBindingValue) {
                    break;
                }
                env = env->outerEnvironment();
            }
            if (env->record()->isObjectEnvironmentRecord()) {
                thisValue = env->record()->asObjectEnvironmentRecord()->bindingObject();
            }
        }
        registerFile[code->m_resultIndex] = Object::call(state, eval, thisValue, argc, argv);
    }
}

NEVER_INLINE void ByteCodeInterpreter::classOperation(ExecutionState& state, CreateClass* code, Value* registerFile)
{
    Value protoParent;
    Value constructorParent;

    // http://www.ecma-international.org/ecma-262/6.0/#sec-runtime-semantics-classdefinitionevaluation

    bool heritagePresent = code->m_superClassRegisterIndex != REGISTER_LIMIT;

    // 5.
    if (!heritagePresent) {
        // 5.a
        protoParent = state.context()->globalObject()->objectPrototype();
        // 5.b
        constructorParent = state.context()->globalObject()->functionPrototype();

    } else {
        // 5.a-c
        const Value& superClass = registerFile[code->m_superClassRegisterIndex];

        if (superClass.isNull()) {
            protoParent = Value(Value::Null);
            constructorParent = state.context()->globalObject()->functionPrototype();
        } else if (!superClass.isConstructor()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Class_Extends_Value_Is_Not_Object_Nor_Null);
        } else {
            if (superClass.isObject() && superClass.asObject()->isGeneratorObject()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Class_Prototype_Is_Not_Object_Nor_Null);
            }

            protoParent = superClass.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().prototype)).value(state, Value());

            if (!protoParent.isObject() && !protoParent.isNull()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Class_Prototype_Is_Not_Object_Nor_Null);
            }

            constructorParent = superClass;
        }
    }

    Object* proto = new Object(state);
    proto->setPrototype(state, protoParent);

    ScriptClassConstructorFunctionObject* constructor;

    if (code->m_codeBlock) {
        constructor = new ScriptClassConstructorFunctionObject(state, code->m_codeBlock, state.mostNearestHeapAllocatedLexicalEnvironment(), proto, code->m_classSrc);
    } else {
        if (!heritagePresent) {
            Value argv[] = { String::emptyString, String::emptyString };
            auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false, false);
            functionSource.codeBlock->setAsClassConstructor();
            constructor = new ScriptClassConstructorFunctionObject(state, functionSource.codeBlock, functionSource.outerEnvironment, proto, code->m_classSrc);
        } else {
            Value argv[] = { new ASCIIString("...args"), new ASCIIString("super(...args)") };
            auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false, true);
            functionSource.codeBlock->setAsClassConstructor();
            functionSource.codeBlock->setAsDerivedClassConstructor();
            constructor = new ScriptClassConstructorFunctionObject(state, functionSource.codeBlock, functionSource.outerEnvironment, proto, code->m_classSrc);
        }
    }

    constructor->setPrototype(state, constructorParent);
    constructor->asFunctionObject()->setFunctionPrototype(state, proto);

    // Perform CreateMethodProperty(proto, "constructor", F).
    // --> CreateMethodProperty: Let newDesc be the PropertyDescriptor{[[Value]]: V, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}.
    proto->defineOwnProperty(state, state.context()->staticStrings().constructor, ObjectPropertyDescriptor(constructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ValuePresent)));

    registerFile[code->m_classConstructorRegisterIndex] = constructor;
    registerFile[code->m_classPrototypeRegisterIndex] = proto;
}

NEVER_INLINE void ByteCodeInterpreter::superOperation(ExecutionState& state, SuperReference* code, Value* registerFile)
{
    if (code->m_isCall) {
        // Let newTarget be GetNewTarget().
        Object* newTarget = state.getNewTarget();
        // If newTarget is undefined, throw a ReferenceError exception.
        if (!newTarget) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_New_Target_Is_Undefined);
        }
        registerFile[code->m_dstIndex] = state.getSuperConstructor();
    } else {
        registerFile[code->m_dstIndex] = state.makeSuperPropertyReference();
    }
}

NEVER_INLINE void ByteCodeInterpreter::superSetObjectOperation(ExecutionState& state, SuperSetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    // find `this` value for receiver
    Value thisValue(Value::ForceUninitialized);
    if (byteCodeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
        EnvironmentRecord* envRec = state.getThisEnvironment();
        ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
        thisValue = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state);
    } else {
        thisValue = registerFile[byteCodeBlock->m_requiredRegisterFileSizeInValueSize];
    }

    Value object = registerFile[code->m_objectRegisterIndex];
    object.toObject(state)->set(state, ObjectPropertyName(state, registerFile[code->m_propertyNameIndex]), registerFile[code->m_loadRegisterIndex], thisValue);
}

NEVER_INLINE Value ByteCodeInterpreter::superGetObjectOperation(ExecutionState& state, SuperGetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    // find `this` value for receiver
    Value thisValue(Value::ForceUninitialized);
    if (byteCodeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
        EnvironmentRecord* envRec = state.getThisEnvironment();
        ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
        thisValue = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state);
    } else {
        thisValue = registerFile[byteCodeBlock->m_requiredRegisterFileSizeInValueSize];
    }

    Value object = registerFile[code->m_objectRegisterIndex];
    return object.toObject(state)->get(state, ObjectPropertyName(state, registerFile[code->m_propertyNameIndex])).value(state, thisValue);
}

NEVER_INLINE void ByteCodeInterpreter::callSuperOperation(ExecutionState& state, CallSuper* code, Value* registerFile)
{
    // Let newTarget be GetNewTarget().
    Object* newTarget = state.getNewTarget();
    // If newTarget is undefined, throw a ReferenceError exception. <-- we checked this at superOperation

    // Let func be GetSuperConstructor(). <-- superOperation did this to calleeIndex

    // Let argList be ArgumentListEvaluation of Arguments.
    // ReturnIfAbrupt(argList).
    size_t argc;
    Value* argv;
    ValueVector spreadArgs;

    if (code->m_hasSpreadElement) {
        spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
        argv = spreadArgs.data();
        argc = spreadArgs.size();
    } else {
        argc = code->m_argumentCount;
        argv = &registerFile[code->m_argumentsStartIndex];
    }

    // Let result be Construct(func, argList, newTarget).
    Value result = Object::construct(state, registerFile[code->m_calleeIndex], argc, argv, newTarget);

    // Let thisER be GetThisEnvironment( ).
    // Return thisER.BindThisValue(result).
    EnvironmentRecord* thisER = state.getThisEnvironment();
    thisER->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->bindThisValue(state, result);
    registerFile[code->m_resultIndex] = result;
}

NEVER_INLINE Value ByteCodeInterpreter::withOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    WithOperation* code = (WithOperation*)programCounter;

    bool inGeneratorScope = state->inGeneratorScope();
    bool inGeneratorResumeProcess = code->m_registerIndex == REGISTER_LIMIT;

    LexicalEnvironment* newEnv;
    if (!LIKELY(inGeneratorResumeProcess)) {
        LexicalEnvironment* env = state->lexicalEnvironment();
        if (!state->ensureRareData()->m_controlFlowRecord) {
            state->ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state->ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        size_t newPc = programCounter + sizeof(WithOperation);
        char* codeBuffer = byteCodeBlock->m_code.data();

        // setup new env
        EnvironmentRecord* newRecord = new ObjectEnvironmentRecord(registerFile[code->m_registerIndex].toObject(*state));
        newEnv = new LexicalEnvironment(newRecord, env);
    } else {
        newEnv = nullptr;
    }

    bool shouldUseHeapAllocatedState = inGeneratorScope && !inGeneratorResumeProcess;
    ExecutionState* newState;

    if (UNLIKELY(shouldUseHeapAllocatedState)) {
        newState = new ExecutionState(state, newEnv, state->inStrictMode());
        newState->ensureRareData();
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, newEnv, state->inStrictMode());
    }

    if (!LIKELY(inGeneratorResumeProcess)) {
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    }

    size_t newPc = programCounter + sizeof(WithOperation);
    char* codeBuffer = byteCodeBlock->m_code.data();
    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);

    if (UNLIKELY(inGeneratorResumeProcess)) {
        state = newState->parent();
        code = (WithOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    }

    ControlFlowRecord* record = state->rareData()->m_controlFlowRecord->back();
    state->rareData()->m_controlFlowRecord->erase(state->rareData()->m_controlFlowRecord->size() - 1);

    if (record) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count() && (record->outerLimitCount() < record->count())) {
                state->rareData()->m_controlFlowRecord->back() = record;
                return Value();
            } else {
                programCounter = jumpTo(codeBuffer, pos);
                return Value(Value::EmptyValue);
            }
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state->rareData()->m_controlFlowRecord->back() = record;
            }
            return record->value();
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_withEndPostion);
        return Value(Value::EmptyValue);
    }
}

NEVER_INLINE void ByteCodeInterpreter::replaceBlockLexicalEnvironmentOperation(ExecutionState& state, size_t programCounter, ByteCodeBlock* byteCodeBlock)
{
    ReplaceBlockLexicalEnvironmentOperation* code = (ReplaceBlockLexicalEnvironmentOperation*)programCounter;
    // setup new env
    EnvironmentRecord* newRecord;
    LexicalEnvironment* newEnv;

    bool shouldUseIndexedStorage = byteCodeBlock->m_codeBlock->canUseIndexedVariableStorage();
    ASSERT(code->m_blockInfo->m_shouldAllocateEnvironment);
    if (LIKELY(shouldUseIndexedStorage)) {
        newRecord = new DeclarativeEnvironmentRecordIndexed(state, code->m_blockInfo);
    } else {
        newRecord = new DeclarativeEnvironmentRecordNotIndexed(state);

        auto& iv = code->m_blockInfo->m_identifiers;
        auto siz = iv.size();
        for (size_t i = 0; i < siz; i++) {
            newRecord->createBinding(state, iv[i].m_name, false, iv[i].m_isMutable, false);
        }
    }
    newEnv = new LexicalEnvironment(newRecord, state.lexicalEnvironment()->outerEnvironment());
    ASSERT(newEnv->isAllocatedOnHeap());

    state.setLexicalEnvironment(newEnv, state.inStrictMode());
}

NEVER_INLINE Value ByteCodeInterpreter::blockOperation(ExecutionState*& state, BlockOperation* code, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    if (!state->ensureRareData()->m_controlFlowRecord) {
        state->ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
    }
    state->ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
    size_t newPc = programCounter + sizeof(BlockOperation);
    char* codeBuffer = byteCodeBlock->m_code.data();

    // setup new env
    EnvironmentRecord* newRecord;
    LexicalEnvironment* newEnv;
    bool inGeneratorResumeProcess = code->m_blockInfo == nullptr;
    bool shouldUseIndexedStorage = byteCodeBlock->m_codeBlock->canUseIndexedVariableStorage();
    bool inGeneratorScope = state->inGeneratorScope();

    if (LIKELY(!inGeneratorResumeProcess)) {
        ASSERT(code->m_blockInfo->m_shouldAllocateEnvironment);
        if (LIKELY(shouldUseIndexedStorage)) {
            newRecord = new DeclarativeEnvironmentRecordIndexed(*state, code->m_blockInfo);
        } else {
            newRecord = new DeclarativeEnvironmentRecordNotIndexed(*state);

            auto& iv = code->m_blockInfo->m_identifiers;
            auto siz = iv.size();
            for (size_t i = 0; i < siz; i++) {
                newRecord->createBinding(*state, iv[i].m_name, false, iv[i].m_isMutable, false);
            }
        }
        newEnv = new LexicalEnvironment(newRecord, state->lexicalEnvironment());
        ASSERT(newEnv->isAllocatedOnHeap());
    } else {
        newRecord = nullptr;
        newEnv = nullptr;
    }

    bool shouldUseHeapAllocatedState = inGeneratorScope && !inGeneratorResumeProcess;
    ExecutionState* newState;
    if (UNLIKELY(shouldUseHeapAllocatedState)) {
        newState = new ExecutionState(state, newEnv, state->inStrictMode());
        newState->ensureRareData();
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, newEnv, state->inStrictMode());
    }

    if (!LIKELY(inGeneratorResumeProcess)) {
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    }

    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);

    if (UNLIKELY(inGeneratorResumeProcess)) {
        state = newState->parent();
        code = (BlockOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    }

    ControlFlowRecord* record = state->rareData()->m_controlFlowRecord->back();
    state->rareData()->m_controlFlowRecord->erase(state->rareData()->m_controlFlowRecord->size() - 1);

    if (record != nullptr) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count() && (record->outerLimitCount() < record->count())) {
                state->rareData()->m_controlFlowRecord->back() = record;
                return Value();
            } else {
                programCounter = jumpTo(codeBuffer, pos);
                return Value(Value::EmptyValue);
            }
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state->rareData()->m_controlFlowRecord->back() = record;
            }
            return record->value();
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_blockEndPosition);
        return Value(Value::EmptyValue);
    }
}

NEVER_INLINE bool ByteCodeInterpreter::binaryInOperation(ExecutionState& state, const Value& left, const Value& right)
{
    if (!right.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "type of rvalue is not Object");
        return false;
    }

    // https://www.ecma-international.org/ecma-262/5.1/#sec-11.8.7
    // Return the result of calling the [[HasProperty]] internal method of rval with argument ToString(lval).
    return right.toObject(state)->hasProperty(state, ObjectPropertyName(state, left));
}

NEVER_INLINE void ByteCodeInterpreter::callFunctionInWithScope(ExecutionState& state, CallFunctionInWithScope* code, Value* registerFile)
{
    const AtomicString& calleeName = code->m_calleeName;
    // NOTE: record for with scope
    Object* receiverObj = NULL;
    Value callee;
    EnvironmentRecord* bindedRecord = getBindedEnvironmentRecordByName(state, state.lexicalEnvironment(), calleeName, callee);
    if (!bindedRecord) {
        callee = Value();
    }

    if (bindedRecord && bindedRecord->isObjectEnvironmentRecord()) {
        receiverObj = bindedRecord->asObjectEnvironmentRecord()->bindingObject();
    } else {
        receiverObj = state.context()->globalObject();
    }

    if (code->m_hasSpreadElement) {
        ValueVector spreadArgs;
        spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
        registerFile[code->m_resultIndex] = Object::call(state, callee, receiverObj, spreadArgs.size(), spreadArgs.data());
    } else {
        registerFile[code->m_resultIndex] = Object::call(state, callee, receiverObj, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
    }
}

NEVER_INLINE void ByteCodeInterpreter::spreadFunctionArguments(ExecutionState& state, const Value* argv, const size_t argc, ValueVector& argVector)
{
    for (size_t i = 0; i < argc; i++) {
        Value arg = argv[i];
        if (arg.isObject() && arg.asObject()->isSpreadArray()) {
            ArrayObject* spreadArray = arg.asObject()->asArrayObject();
            ASSERT(spreadArray->isFastModeArray());
            for (size_t i = 0; i < spreadArray->getArrayLength(state); i++) {
                argVector.push_back(spreadArray->m_fastModeData[i]);
            }
        } else {
            argVector.push_back(arg);
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::yieldOperationImplementation(ExecutionState& state, Value returnValue, size_t tailDataPosition, size_t tailDataLength, size_t nextProgramCounter, ByteCodeRegisterIndex dstRegisterIndex, bool isDelegateOperation)
{
    ASSERT(state.inGeneratorScope());

    ExecutionState* generatorOriginalState = &state;
    GeneratorObject* gen = nullptr;
    while (true) {
        gen = generatorOriginalState->generatorTarget();
        if (gen) {
            generatorOriginalState->rareData()->m_parent = nullptr;
            break;
        }
        generatorOriginalState = generatorOriginalState->parent();
    }
    gen->m_executionState = &state;
    gen->m_generatorState = GeneratorState::SuspendedYield;
    gen->m_byteCodePosition = nextProgramCounter;
    gen->m_resumeValueIdx = dstRegisterIndex;

    // read & fill recursive statement data
    char* start = (char*)(tailDataPosition);
    char* end = (char*)(start + tailDataLength);

    size_t startupDataPosition = gen->m_byteCodeBlock->m_code.size();
    gen->m_extraDataByteCodePosition = startupDataPosition;
    std::vector<size_t> codeStartPositions;
    size_t codePos = startupDataPosition;
    while (start != end) {
        size_t e = *((size_t*)start);
        start += sizeof(ByteCodeGenerateContext::RecursiveStatementKind);
        size_t startPos = *((size_t*)start);
        codeStartPositions.push_back(startPos);
        if (e == ByteCodeGenerateContext::Block) {
            gen->m_byteCodeBlock->m_code.resize(gen->m_byteCodeBlock->m_code.size() + sizeof(BlockOperation));
            BlockOperation* code = new (gen->m_byteCodeBlock->m_code.data() + codePos) BlockOperation(ByteCodeLOC(SIZE_MAX), nullptr);
            code->assignOpcodeInAddress();

            codePos += sizeof(BlockOperation);
        } else if (e == ByteCodeGenerateContext::With) {
            gen->m_byteCodeBlock->m_code.resize(gen->m_byteCodeBlock->m_code.size() + sizeof(WithOperation));
            WithOperation* code = new (gen->m_byteCodeBlock->m_code.data() + codePos) WithOperation(ByteCodeLOC(SIZE_MAX), REGISTER_LIMIT);
            code->assignOpcodeInAddress();

            codePos += sizeof(WithOperation);
        } else if (e == ByteCodeGenerateContext::Try) {
            gen->m_byteCodeBlock->m_code.resize(gen->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (gen->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isTryResumeProcess = true;

            codePos += sizeof(TryOperation);
        } else if (e == ByteCodeGenerateContext::Catch) {
            gen->m_byteCodeBlock->m_code.resize(gen->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (gen->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isCatchResumeProcess = true;

            codePos += sizeof(TryOperation);
        } else {
            ASSERT(e == ByteCodeGenerateContext::Finally);
            gen->m_byteCodeBlock->m_code.resize(gen->m_byteCodeBlock->m_code.size() + sizeof(TryOperation));
            TryOperation* code = new (gen->m_byteCodeBlock->m_code.data() + codePos) TryOperation(ByteCodeLOC(SIZE_MAX));
            code->assignOpcodeInAddress();
            code->m_isFinallyResumeProcess = true;

            codePos += sizeof(TryOperation);
        }
        start += sizeof(size_t); // start pos
    }

    size_t resumeCodePos = gen->m_byteCodeBlock->m_code.size();
    gen->m_generatorResumeByteCodePosition = resumeCodePos;
    gen->m_byteCodeBlock->m_code.resizeWithUninitializedValues(resumeCodePos + sizeof(GeneratorResume));
    auto resumeCode = new (gen->m_byteCodeBlock->m_code.data() + resumeCodePos) GeneratorResume(ByteCodeLOC(SIZE_MAX), gen);
    resumeCode->assignOpcodeInAddress();

    size_t stackSizePos = gen->m_byteCodeBlock->m_code.size();
    gen->m_byteCodeBlock->m_code.resizeWithUninitializedValues(stackSizePos + sizeof(size_t));
    new (gen->m_byteCodeBlock->m_code.data() + stackSizePos) size_t(codeStartPositions.size());

    // add ByteCodePositions
    for (size_t i = 0; i < codeStartPositions.size(); i++) {
        size_t pos = gen->m_byteCodeBlock->m_code.size();
        gen->m_byteCodeBlock->m_code.resizeWithUninitializedValues(pos + sizeof(size_t));
        new (gen->m_byteCodeBlock->m_code.data() + pos) size_t(codeStartPositions[i]);
    }

    GeneratorObject::GeneratorExitValue* exitValue = new GeneratorObject::GeneratorExitValue();
    exitValue->m_value = returnValue;
    exitValue->m_isDelegateOperation = isDelegateOperation;
    throw exitValue;
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-runtime-semantics-evaluation
NEVER_INLINE void ByteCodeInterpreter::yieldOperation(ExecutionState& state, Value* registerFile, size_t programCounter, char* codeBuffer)
{
    ASSERT(state.inGeneratorScope());

    Yield* code = (Yield*)programCounter;
    auto yieldIndex = code->m_yieldIdx;
    Value ret = yieldIndex == REGISTER_LIMIT ? Value() : registerFile[yieldIndex];
    auto dstIdx = code->m_dstIdx;

    size_t nextProgramCounter = programCounter - (size_t)codeBuffer + sizeof(Yield) + code->m_tailDataLength;
    yieldOperationImplementation(state, ret, programCounter + sizeof(Yield), code->m_tailDataLength, nextProgramCounter, dstIdx, false);
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-runtime-semantics-evaluation
NEVER_INLINE Value ByteCodeInterpreter::yieldDelegateOperation(ExecutionState& state, Value* registerFile, size_t& programCounter, char* codeBuffer)
{
    YieldDelegate* code = (YieldDelegate*)programCounter;
    const Value iterator = registerFile[code->m_iterIdx].toObject(state);
    GeneratorState resultState = GeneratorState::SuspendedYield;
    Value nextResult;
    bool done = true;
    Value nextValue;
    try {
        nextResult = iteratorNext(state, iterator);
        if (iterator.asObject()->isGeneratorObject()) {
            resultState = iterator.asObject()->asGeneratorObject()->m_generatorState;
        }
    } catch (const Value& v) {
        resultState = GeneratorState::CompletedThrow;
        nextValue = v;
    }

    switch (resultState) {
    case GeneratorState::CompletedReturn: {
        Value ret = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().stringReturn)).value(state, iterator);

        nextValue = iteratorValue(state, nextResult);

        if (ret.isUndefined()) {
            return nextValue;
        }

        Value innerResult = Object::call(state, ret, iterator, 1, &nextValue);

        if (!innerResult.isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "IteratorResult is not an object");
        }

        nextValue = iteratorValue(state, innerResult);
        done = iteratorComplete(state, innerResult);
        break;
    }
    case GeneratorState::SuspendedYield: {
        done = iteratorComplete(state, nextResult);
        nextValue = iteratorValue(state, nextResult);
        break;
    }
    default: {
        ASSERT(resultState == GeneratorState::CompletedThrow);
        Value throwMethod = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().stringThrow)).value(state, iterator);

        if (!throwMethod.isUndefined()) {
            Value innerResult;
            innerResult = Object::call(state, throwMethod, iterator, 1, &nextValue);
            if (!innerResult.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "IteratorResult is not an object");
            }
            nextValue = iteratorValue(state, innerResult);
            done = iteratorComplete(state, innerResult);
        } else {
            iteratorClose(state, iterator, Value(), false);
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "yield* violation");
        }
    }
    }

    // yield
    registerFile[code->m_dstIdx] = nextValue;
    if (done) {
        programCounter = jumpTo(codeBuffer, code->m_endPosition);
        return Value(Value::EmptyValue);
    }

    registerFile[code->m_valueIdx] = nextValue;

    size_t nextProgramCounter = programCounter - (size_t)codeBuffer + sizeof(YieldDelegate) + code->m_tailDataLength;
    yieldOperationImplementation(state, nextResult, programCounter + sizeof(YieldDelegate), code->m_tailDataLength, nextProgramCounter, REGISTER_LIMIT, true);

    ASSERT_NOT_REACHED();
}

NEVER_INLINE Value ByteCodeInterpreter::generatorResumeOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock)
{
    GeneratorResume* code = (GeneratorResume*)programCounter;

    bool needsReturn = code->m_needsReturn;
    bool needsThrow = code->m_needsThrow;

    GeneratorObject* gen = code->m_generatorObject;
    // update old parent
    ExecutionState* orgTreePointer = gen->m_executionState;
    ExecutionState* tmpTreePointer = state;

    size_t pos = programCounter + sizeof(GeneratorResume);
    size_t recursiveStatementCodeStackSize = *((size_t*)pos);
    pos += sizeof(size_t) + sizeof(size_t) * recursiveStatementCodeStackSize;

    for (size_t i = 0; i < recursiveStatementCodeStackSize; i++) {
        pos -= sizeof(size_t);
        size_t codePos = *((size_t*)pos);

#ifndef NDEBUG
        if (orgTreePointer->hasRareData() && orgTreePointer->generatorTarget()) {
            // this ExecutuionState must be allocated by generatorResume function;
            ASSERT(tmpTreePointer->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->isGenerator());
        }
#endif
        auto tmpTreePointerSave = tmpTreePointer;
        auto orgTreePointerSave = orgTreePointer;
        orgTreePointer = orgTreePointer->parent();
        tmpTreePointer = tmpTreePointer->parent();

        tmpTreePointerSave->setParent(orgTreePointerSave->parent());
        tmpTreePointerSave->ensureRareData()->m_programCounterWhenItStoppedByYield = codePos;
    }

    state = code->m_generatorObject->m_executionState;

    // remove extra code
    byteCodeBlock->m_code.resize(code->m_generatorObject->m_extraDataByteCodePosition);

    // update program counter
    programCounter = code->m_generatorObject->m_byteCodePosition + (size_t)byteCodeBlock->m_code.data();

    if (needsReturn) {
        if (state->rareData() && state->rareData()->m_controlFlowRecord && state->rareData()->m_controlFlowRecord->size()) {
            state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, gen->m_resumeValue, state->rareData()->m_controlFlowRecord->size());
        }
        return gen->m_resumeValue;
    } else if (needsThrow) {
        state->throwException(gen->m_resumeValue);
    }

    return Value(Value::EmptyValue);
}

NEVER_INLINE void ByteCodeInterpreter::newTargetOperation(ExecutionState& state, NewTargetOperation* code, Value* registerFile)
{
    auto newTarget = state.getNewTarget();
    if (newTarget) {
        registerFile[code->m_registerIndex] = state.getNewTarget();
    } else {
        registerFile[code->m_registerIndex] = Value();
    }
}

static Value createObjectPropertyFunctionName(ExecutionState& state, const Value& name, const char* prefix)
{
    StringBuilder builder;
    if (name.isSymbol()) {
        builder.appendString(prefix);
        builder.appendString("[");
        builder.appendString(name.asSymbol()->description());
        builder.appendString("]");
    } else {
        builder.appendString(prefix);
        builder.appendString(name.toString(state));
    }
    return builder.finalize(&state);
}

NEVER_INLINE void ByteCodeInterpreter::objectDefineOwnPropertyOperation(ExecutionState& state, ObjectDefineOwnPropertyOperation* code, Value* registerFile)
{
    const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
    const Value& property = registerFile[code->m_propertyRegisterIndex];
    const Value& value = registerFile[code->m_loadRegisterIndex];

    Value propertyStringOrSymbol = property.isSymbol() ? property : property.toString(state);

    if (code->m_needsToRedefineFunctionNameOnValue) {
        Value fnName = createObjectPropertyFunctionName(state, propertyStringOrSymbol, "");
        value.asFunction()->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(fnName));
    }

    ObjectPropertyName objPropName = ObjectPropertyName(state, propertyStringOrSymbol);
    // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
    if (propertyStringOrSymbol.isString() && propertyStringOrSymbol.asString()->equals("__proto__")) {
        willBeObject.asObject()->setPrototype(state, value);
    } else {
        willBeObject.asObject()->defineOwnProperty(state, objPropName, ObjectPropertyDescriptor(value, code->m_presentAttribute));
    }
}

NEVER_INLINE void ByteCodeInterpreter::objectDefineOwnPropertyWithNameOperation(ExecutionState& state, ObjectDefineOwnPropertyWithNameOperation* code, Value* registerFile)
{
    const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
    // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
    if (code->m_propertyName == state.context()->staticStrings().__proto__) {
        willBeObject.asObject()->setPrototype(state, registerFile[code->m_loadRegisterIndex]);
    } else {
        willBeObject.asObject()->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], code->m_presentAttribute));
    }
}

NEVER_INLINE void ByteCodeInterpreter::arrayDefineOwnPropertyOperation(ExecutionState& state, ArrayDefineOwnPropertyOperation* code, Value* registerFile)
{
    ArrayObject* arr = registerFile[code->m_objectRegisterIndex].asObject()->asArrayObject();
    if (LIKELY(arr->isFastModeArray())) {
        for (size_t i = 0; i < code->m_count; i++) {
            if (LIKELY(code->m_loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                arr->m_fastModeData[i + code->m_baseIndex] = registerFile[code->m_loadRegisterIndexs[i]];
            }
        }
    } else {
        for (size_t i = 0; i < code->m_count; i++) {
            if (LIKELY(code->m_loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                const Value& element = registerFile[code->m_loadRegisterIndexs[i]];
                arr->defineOwnProperty(state, ObjectPropertyName(state, i + code->m_baseIndex), ObjectPropertyDescriptor(element, ObjectPropertyDescriptor::AllPresent));
            }
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::arrayDefineOwnPropertyBySpreadElementOperation(ExecutionState& state, ArrayDefineOwnPropertyBySpreadElementOperation* code, Value* registerFile)
{
    ArrayObject* arr = registerFile[code->m_objectRegisterIndex].asObject()->asArrayObject();

    if (LIKELY(arr->isFastModeArray())) {
        size_t baseIndex = arr->getArrayLength(state);
        size_t elementLength = code->m_count;
        for (size_t i = 0; i < code->m_count; i++) {
            if (code->m_loadRegisterIndexs[i] != REGISTER_LIMIT) {
                Value element = registerFile[code->m_loadRegisterIndexs[i]];
                if (element.isObject() && element.asObject()->isSpreadArray()) {
                    elementLength = elementLength + element.asObject()->asArrayObject()->getArrayLength(state) - 1;
                }
            }
        }

        size_t newLength = baseIndex + elementLength;
        arr->setArrayLength(state, newLength);
        ASSERT(arr->isFastModeArray());

        size_t elementIndex = 0;
        for (size_t i = 0; i < code->m_count; i++) {
            if (LIKELY(code->m_loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                Value element = registerFile[code->m_loadRegisterIndexs[i]];
                if (element.isObject() && element.asObject()->isSpreadArray()) {
                    ArrayObject* spreadArray = element.asObject()->asArrayObject();
                    ASSERT(spreadArray->isFastModeArray());
                    for (size_t spreadIndex = 0; spreadIndex < spreadArray->getArrayLength(state); spreadIndex++) {
                        arr->m_fastModeData[baseIndex + elementIndex] = spreadArray->m_fastModeData[spreadIndex];
                        elementIndex++;
                    }
                } else {
                    arr->m_fastModeData[baseIndex + elementIndex] = element;
                    elementIndex++;
                }
            } else {
                elementIndex++;
            }
        }

        ASSERT(elementIndex == elementLength);

    } else {
        ByteCodeRegisterIndex objectRegisterIndex = code->m_objectRegisterIndex;
        size_t count = code->m_count;
        ByteCodeRegisterIndex* loadRegisterIndexs = code->m_loadRegisterIndexs;
        ArrayObject* arr = registerFile[objectRegisterIndex].asObject()->asArrayObject();
        ASSERT(!arr->isFastModeArray());

        size_t baseIndex = arr->getArrayLength(state);
        size_t elementIndex = 0;

        for (size_t i = 0; i < count; i++) {
            if (LIKELY(loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                Value element = registerFile[loadRegisterIndexs[i]];
                if (element.isObject() && element.asObject()->isSpreadArray()) {
                    ArrayObject* spreadArray = element.asObject()->asArrayObject();
                    ASSERT(spreadArray->isFastModeArray());
                    Value spreadElement;
                    for (size_t spreadIndex = 0; spreadIndex < spreadArray->getArrayLength(state); spreadIndex++) {
                        spreadElement = spreadArray->m_fastModeData[spreadIndex];
                        arr->defineOwnProperty(state, ObjectPropertyName(state, baseIndex + elementIndex), ObjectPropertyDescriptor(spreadElement, ObjectPropertyDescriptor::AllPresent));
                        elementIndex++;
                    }
                } else {
                    arr->defineOwnProperty(state, ObjectPropertyName(state, baseIndex + elementIndex), ObjectPropertyDescriptor(element, ObjectPropertyDescriptor::AllPresent));
                    elementIndex++;
                }
            } else {
                elementIndex++;
            }
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::createSpreadArrayObject(ExecutionState& state, CreateSpreadArrayObject* code, Value* registerFile)
{
    ArrayObject* spreadArray = ArrayObject::createSpreadArray(state);
    ASSERT(spreadArray->isFastModeArray());

    Value iterator = getIterator(state, registerFile[code->m_argumentIndex]);
    size_t i = 0;
    while (true) {
        Value next = iteratorStep(state, iterator);
        if (next.isFalse()) {
            break;
        }
        Value value = iteratorValue(state, next);
        spreadArray->setIndexedProperty(state, Value(i++), iteratorValue(state, next));
    }
    registerFile[code->m_registerIndex] = spreadArray;
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectGetterSetter(ExecutionState& state, ObjectDefineGetterSetter* code, Value* registerFile)
{
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    Value pName = registerFile[code->m_objectPropertyNameRegisterIndex];
    Value fnName;
    if (code->m_isGetter) {
        fnName = createObjectPropertyFunctionName(state, pName, "get ");
    } else {
        Value fnName = createObjectPropertyFunctionName(state, pName, "set ");
    }
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(fnName));
    JSGetterSetter* gs;
    if (code->m_isGetter) {
        gs = new (alloca(sizeof(JSGetterSetter))) JSGetterSetter(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
    } else {
        gs = new (alloca(sizeof(JSGetterSetter))) JSGetterSetter(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
    }
    ObjectPropertyDescriptor desc(*gs, code->m_presentAttribute);
    Object* object = registerFile[code->m_objectRegisterIndex].toObject(state);
    object->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, pName), desc);
}

ALWAYS_INLINE Value ByteCodeInterpreter::incrementOperation(ExecutionState& state, const Value& value)
{
    if (LIKELY(value.isInt32())) {
        int32_t a = value.asInt32();
        int32_t b = 1;
        int32_t c;
        bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::add(a, b, c);
        if (LIKELY(result)) {
            return Value(c);
        } else {
            return Value(Value::EncodeAsDouble, (double)a + (double)b);
        }
    } else {
        return plusSlowCase(state, Value(value.toNumber(state)), Value(1));
    }
}

ALWAYS_INLINE Value ByteCodeInterpreter::decrementOperation(ExecutionState& state, const Value& value)
{
    if (LIKELY(value.isInt32())) {
        int32_t a = value.asInt32();
        int32_t b = -1;
        int32_t c;
        bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::add(a, b, c);
        if (LIKELY(result)) {
            return Value(c);
        } else {
            return Value(Value::EncodeAsDouble, (double)a + (double)b);
        }
    } else {
        return Value(value.toNumber(state) - 1);
    }
}

NEVER_INLINE void ByteCodeInterpreter::unaryTypeof(ExecutionState& state, UnaryTypeof* code, Value* registerFile)
{
    Value val;
    if (code->m_id.string()->length()) {
        val = loadByName(state, state.lexicalEnvironment(), code->m_id, false);
    } else {
        val = registerFile[code->m_srcIndex];
    }
    if (val.isUndefined()) {
        val = state.context()->staticStrings().undefined.string();
    } else if (val.isNull()) {
        val = state.context()->staticStrings().object.string();
    } else if (val.isBoolean()) {
        val = state.context()->staticStrings().boolean.string();
    } else if (val.isNumber()) {
        val = state.context()->staticStrings().number.string();
    } else if (val.isString()) {
        val = state.context()->staticStrings().string.string();
    } else {
        ASSERT(val.isPointerValue());
        PointerValue* p = val.asPointerValue();
        if (p->isCallable()) {
            val = state.context()->staticStrings().function.string();
        } else if (p->isSymbol()) {
            val = state.context()->staticStrings().symbol.string();
        } else {
            val = state.context()->staticStrings().object.string();
        }
    }

    registerFile[code->m_dstIndex] = val;
}

NEVER_INLINE void ByteCodeInterpreter::checkIfKeyIsLast(ExecutionState& state, CheckIfKeyIsLast* code, char* codeBuffer, size_t& programCounter, Value* registerFile)
{
    EnumerateObjectData* data = (EnumerateObjectData*)registerFile[code->m_registerIndex].asPointerValue();
    bool shouldUpdateEnumerateObjectData = false;
    Object* obj = data->m_object;
    for (size_t i = 0; i < data->m_hiddenClassChain.size(); i++) {
        auto hc = data->m_hiddenClassChain[i];
        ObjectStructureChainItem testItem;
        testItem.m_objectStructure = obj->structure();
        if (hc != testItem) {
            shouldUpdateEnumerateObjectData = true;
            break;
        }
        Value val = obj->getPrototype(state);
        if (val.isObject()) {
            obj = val.asObject();
        } else {
            break;
        }
    }

    if (!shouldUpdateEnumerateObjectData && data->m_object->isArrayObject() && data->m_object->length(state) != data->m_originalLength) {
        shouldUpdateEnumerateObjectData = true;
    }
    if (!shouldUpdateEnumerateObjectData && data->m_object->rareData() && data->m_object->rareData()->m_shouldUpdateEnumerateObjectData) {
        shouldUpdateEnumerateObjectData = true;
    }

    if (shouldUpdateEnumerateObjectData) {
        registerFile[code->m_registerIndex] = Value((PointerValue*)updateEnumerateObjectData(state, data));
        data = (EnumerateObjectData*)registerFile[code->m_registerIndex].asPointerValue();
    }

    if (data->m_keys.size() <= data->m_idx) {
        programCounter = jumpTo(codeBuffer, code->m_forInEndPosition);
    } else {
        ADD_PROGRAM_COUNTER(CheckIfKeyIsLast);
    }
}

NEVER_INLINE void ByteCodeInterpreter::getIteratorOperation(ExecutionState& state, GetIterator* code, Value* registerFile)
{
    const Value& obj = registerFile[code->m_objectRegisterIndex];
    registerFile[code->m_registerIndex] = getIterator(state, obj);
}

NEVER_INLINE void ByteCodeInterpreter::iteratorStepOperation(ExecutionState& state, size_t& programCounter, Value* registerFile, char* codeBuffer)
{
    IteratorStep* code = (IteratorStep*)programCounter;
    Value nextResult = iteratorStep(state, registerFile[code->m_iterRegisterIndex]);

    if (nextResult.isFalse()) {
        if (code->m_forOfEndPosition == SIZE_MAX) {
            registerFile[code->m_registerIndex] = Value();
            ADD_PROGRAM_COUNTER(IteratorStep);
        } else {
            programCounter = jumpTo(codeBuffer, code->m_forOfEndPosition);
        }
    } else {
        registerFile[code->m_registerIndex] = iteratorValue(state, nextResult);
        ADD_PROGRAM_COUNTER(IteratorStep);
    }
}

NEVER_INLINE void ByteCodeInterpreter::iteratorCloseOperation(ExecutionState& state, IteratorClose* code, Value* registerFile)
{
    bool exceptionWasThrown = state.hasRareData() && state.rareData()->m_controlFlowRecord && state.rareData()->m_controlFlowRecord->back() && state.rareData()->m_controlFlowRecord->back()->reason() == ControlFlowRecord::NeedsThrow;
    const Value& iterator = registerFile[code->m_iterRegisterIndex];
    Value returnFunction = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().stringReturn)).value(state, iterator.asObject());
    if (returnFunction.isUndefined()) {
        return;
    }

    Value innerResult;
    bool innerResultHasException = false;
    try {
        innerResult = Object::call(state, returnFunction, iterator, 0, nullptr);
    } catch (const Value& e) {
        innerResult = e;
        innerResultHasException = true;
    }
    if (innerResultHasException) {
        state.throwException(innerResult);
    }
    if (!exceptionWasThrown && !innerResult.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Iterator close result is not an object");
    }
}

NEVER_INLINE void ByteCodeInterpreter::getObjectOpcodeSlowCase(ExecutionState& state, GetObject* code, Value* registerFile)
{
    const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
    const Value& property = registerFile[code->m_propertyRegisterIndex];
    Object* obj;
    if (LIKELY(willBeObject.isObject())) {
        obj = willBeObject.asObject();
    } else {
        obj = fastToObject(state, willBeObject);
    }
    registerFile[code->m_storeRegisterIndex] = obj->getIndexedProperty(state, property).value(state, willBeObject);
}

NEVER_INLINE void ByteCodeInterpreter::setObjectOpcodeSlowCase(ExecutionState& state, SetObjectOperation* code, Value* registerFile)
{
    const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
    const Value& property = registerFile[code->m_propertyRegisterIndex];
    Object* obj = willBeObject.toObject(state);
    if (willBeObject.isPrimitive()) {
        obj->preventExtensions(state);
    }

    bool result = obj->setIndexedProperty(state, property, registerFile[code->m_loadRegisterIndex]);
    if (UNLIKELY(!result) && state.inStrictMode()) {
        Object::throwCannotWriteError(state, PropertyName(state, property.toString(state)));
    }
}

NEVER_INLINE void ByteCodeInterpreter::ensureArgumentsObjectOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    ExecutionState* es = &state;
    while (es) {
        if (es->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && es->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            break;
        }
        es = es->parent();
    }

    auto r = es->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
    auto functionObject = r->functionObject()->asScriptFunctionObject();
    bool isMapped = !functionObject->codeBlock()->hasParameterOtherThanIdentifier() && !functionObject->codeBlock()->isStrict();
    r->functionObject()->asScriptFunctionObject()->generateArgumentsObject(state, state.argc(), state.argv(), r, registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize, isMapped);
}
}
