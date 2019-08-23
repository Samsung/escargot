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
#include "runtime/SpreadObject.h"
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

Value ByteCodeInterpreter::interpret(ExecutionState& state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile)
{
#if defined(COMPILER_GCC)
    if (UNLIKELY(byteCodeBlock == nullptr)) {
        goto FillOpcodeTableLbl;
    }
#endif

    ASSERT(byteCodeBlock != nullptr);
    ASSERT(registerFile != nullptr);

    {
        ExecutionStateProgramCounterBinder binder(state, &programCounter);
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
            ASSERT(byteCodeBlock->m_codeBlock->context() == state.context());
            Context* ctx = state.context();
            GlobalObject* globalObject = state.context()->globalObject();
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
                        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, ctx->globalDeclarativeRecord()[idx].m_name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
                    }
                    registerFile[code->m_registerIndex] = val;
                }
            }
            if (UNLIKELY(!isCacheWork)) {
                registerFile[code->m_registerIndex] = getGlobalVariableSlowCase(state, globalObject, slot, byteCodeBlock);
            }
            ADD_PROGRAM_COUNTER(GetGlobalVariable);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetGlobalVariable)
            :
        {
            SetGlobalVariable* code = (SetGlobalVariable*)programCounter;
            ASSERT(byteCodeBlock->m_codeBlock->context() == state.context());
            Context* ctx = state.context();
            GlobalObject* globalObject = state.context()->globalObject();
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
                        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, ctx->globalDeclarativeRecord()[idx].m_name.string(), false, String::emptyString, errorMessage_IsNotInitialized);
                    }
                    ctx->globalDeclarativeStorage()[idx] = registerFile[code->m_registerIndex];
                }
            }

            if (UNLIKELY(!isCacheWork)) {
                setGlobalVariableSlowCase(state, globalObject, slot, registerFile[code->m_registerIndex], byteCodeBlock);
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
                ret = plusSlowCase(state, v0, v1);
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
                ret = Value(left.toNumber(state) - right.toNumber(state));
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
                auto first = left.toNumber(state);
                auto second = right.toNumber(state);
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
            registerFile[code->m_dstIndex] = Value(left.toNumber(state) / right.toNumber(state));
            ADD_PROGRAM_COUNTER(BinaryDivision);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryEqual)
            :
        {
            BinaryEqual* code = (BinaryEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryNotEqual)
            :
        {
            BinaryNotEqual* code = (BinaryNotEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryStrictEqual)
            :
        {
            BinaryStrictEqual* code = (BinaryStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryStrictEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryNotStrictEqual)
            :
        {
            BinaryNotStrictEqual* code = (BinaryNotStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotStrictEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLessThan)
            :
        {
            BinaryLessThan* code = (BinaryLessThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLessThanOrEqual)
            :
        {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThan)
            :
        {
            BinaryGreaterThan* code = (BinaryGreaterThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThanOrEqual)
            :
        {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ToNumberIncrement)
            :
        {
            ToNumberIncrement* code = (ToNumberIncrement*)programCounter;
            Value toNumberValue = Value(registerFile[code->m_srcIndex].toNumber(state));
            registerFile[code->m_dstIndex] = toNumberValue;
            registerFile[code->m_storeIndex] = incrementOperation(state, toNumberValue);
            ADD_PROGRAM_COUNTER(ToNumberIncrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Increment)
            :
        {
            Increment* code = (Increment*)programCounter;
            registerFile[code->m_dstIndex] = incrementOperation(state, registerFile[code->m_srcIndex]);
            ADD_PROGRAM_COUNTER(Increment);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ToNumberDecrement)
            :
        {
            ToNumberDecrement* code = (ToNumberDecrement*)programCounter;
            Value toNumberValue = Value(registerFile[code->m_srcIndex].toNumber(state));
            registerFile[code->m_dstIndex] = toNumberValue;
            registerFile[code->m_storeIndex] = decrementOperation(state, toNumberValue);
            ADD_PROGRAM_COUNTER(ToNumberDecrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Decrement)
            :
        {
            Decrement* code = (Decrement*)programCounter;
            registerFile[code->m_dstIndex] = decrementOperation(state, registerFile[code->m_srcIndex]);
            ADD_PROGRAM_COUNTER(Decrement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryMinus)
            :
        {
            UnaryMinus* code = (UnaryMinus*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(-val.toNumber(state));
            ADD_PROGRAM_COUNTER(UnaryMinus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryNot)
            :
        {
            UnaryNot* code = (UnaryNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(!val.toBoolean(state));
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
                    uint32_t idx = property.tryToUseAsArrayIndex(state);
                    if (LIKELY(idx != Value::InvalidArrayIndexValue) && LIKELY(idx < arr->getArrayLength(state))) {
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
                uint32_t idx = property.tryToUseAsArrayIndex(state);
                if (LIKELY(arr->isFastModeArray())) {
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        uint32_t len = arr->getArrayLength(state);
                        if (UNLIKELY(len <= idx)) {
                            if (UNLIKELY(!arr->isExtensible(state))) {
                                JUMP_INSTRUCTION(SetObjectOpcodeSlowCase);
                            }
                            if (UNLIKELY(!arr->setArrayLength(state, idx + 1)) || UNLIKELY(!arr->isFastModeArray())) {
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
                obj = fastToObject(state, willBeObject);
            }
            registerFile[code->m_storeRegisterIndex] = getObjectPrecomputedCaseOperation(state, obj, willBeObject, code->m_propertyName, code->m_inlineCache, byteCodeBlock);
            ADD_PROGRAM_COUNTER(GetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetObjectPreComputedCase)
            :
        {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)programCounter;
            setObjectPreComputedCaseOperation(state, registerFile[code->m_objectRegisterIndex], code->m_propertyName, registerFile[code->m_loadRegisterIndex], *code->m_inlineCache, byteCodeBlock);
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
                relation = abstractRelationalComparisonOrEqual(state, left, right, code->m_isLeftFirst);
            } else {
                relation = abstractRelationalComparison(state, left, right, code->m_isLeftFirst);
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
                equality = left.equalsTo(state, right);
            } else {
                equality = left.abstractEqualsTo(state, right);
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
            if (registerFile[code->m_registerIndex].toBoolean(state)) {
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
            if (!registerFile[code->m_registerIndex].toBoolean(state)) {
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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_NOT_Callable);
            }
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(state, Value(), code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);

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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_NOT_Callable);
            }
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(state, receiver, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);

            ADD_PROGRAM_COUNTER(CallFunctionWithReceiver);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadByHeapIndex)
            :
        {
            LoadByHeapIndex* code = (LoadByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = state.lexicalEnvironment();
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            registerFile[code->m_registerIndex] = upperEnv->record()->asDeclarativeEnvironmentRecord()->getHeapValueByIndex(state, code->m_index);
            ADD_PROGRAM_COUNTER(LoadByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(StoreByHeapIndex)
            :
        {
            StoreByHeapIndex* code = (StoreByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = state.lexicalEnvironment();
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            upperEnv->record()->setMutableBindingByIndex(state, code->m_index, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMod)
            :
        {
            BinaryMod* code = (BinaryMod*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = modOperation(state, left, right);
            ADD_PROGRAM_COUNTER(BinaryMod);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseAnd)
            :
        {
            BinaryBitwiseAnd* code = (BinaryBitwiseAnd*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) & right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseAnd);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseOr)
            :
        {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) | right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseOr);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseXor)
            :
        {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) ^ right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseXor);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLeftShift)
            :
        {
            BinaryLeftShift* code = (BinaryLeftShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            int32_t lnum = left.toInt32(state);
            int32_t rnum = right.toInt32(state);
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
            int32_t lnum = left.toInt32(state);
            int32_t rnum = right.toInt32(state);
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
            uint32_t lnum = left.toUint32(state);
            uint32_t rnum = right.toUint32(state);
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
            registerFile[code->m_dstIndex] = Value(~val.toInt32(state));
            ADD_PROGRAM_COUNTER(UnaryBitwiseNot);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ReturnFunctionWithValue)
            :
        {
            ReturnFunctionWithValue* code = (ReturnFunctionWithValue*)programCounter;
            return registerFile[code->m_registerIndex];
        }

        DEFINE_OPCODE(ReturnFunction)
            :
        {
            return Value();
        }

        DEFINE_OPCODE(ToNumber)
            :
        {
            ToNumber* code = (ToNumber*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(val.toNumber(state));
            ADD_PROGRAM_COUNTER(ToNumber);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeGlobalVariable)
            :
        {
            InitializeGlobalVariable* code = (InitializeGlobalVariable*)programCounter;
            ASSERT(byteCodeBlock->m_codeBlock->context() == state.context());
            initializeGlobalVariable(state, code, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeGlobalVariable);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeByHeapIndex)
            :
        {
            InitializeByHeapIndex* code = (InitializeByHeapIndex*)programCounter;
            state.lexicalEnvironment()->record()->initializeBindingByIndex(state, code->m_index, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeByHeapIndex);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateObject)
            :
        {
            CreateObject* code = (CreateObject*)programCounter;
            registerFile[code->m_registerIndex] = new Object(state);
            ADD_PROGRAM_COUNTER(CreateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateArray)
            :
        {
            CreateArray* code = (CreateArray*)programCounter;
            ArrayObject* arr = new ArrayObject(state, code->m_hasSpreadElement);
            arr->setArrayLength(state, code->m_length);
            registerFile[code->m_registerIndex] = arr;
            ADD_PROGRAM_COUNTER(CreateArray);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineOwnPropertyOperation)
            :
        {
            ObjectDefineOwnPropertyOperation* code = (ObjectDefineOwnPropertyOperation*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            ObjectPropertyName objPropName = ObjectPropertyName(state, property);
            // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
            if (property.isString() && property.asString()->equals("__proto__")) {
                willBeObject.asObject()->setPrototype(state, registerFile[code->m_loadRegisterIndex]);
            } else {
                willBeObject.asObject()->defineOwnProperty(state, objPropName, ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], code->m_presentAttribute));
            }
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineOwnPropertyWithNameOperation)
            :
        {
            ObjectDefineOwnPropertyWithNameOperation* code = (ObjectDefineOwnPropertyWithNameOperation*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
            if (code->m_propertyName == state.context()->staticStrings().__proto__) {
                willBeObject.asObject()->setPrototype(state, registerFile[code->m_loadRegisterIndex]);
            } else {
                willBeObject.asObject()->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], code->m_presentAttribute));
            }
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyWithNameOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ArrayDefineOwnPropertyOperation)
            :
        {
            ArrayDefineOwnPropertyOperation* code = (ArrayDefineOwnPropertyOperation*)programCounter;
            ArrayObject* arr = registerFile[code->m_objectRegisterIndex].asObject()->asArrayObject();
            if (LIKELY(arr->isFastModeArray())) {
                size_t end = code->m_count + code->m_baseIndex;
                for (size_t i = 0; i < code->m_count; i++) {
                    if (LIKELY(code->m_loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                        arr->m_fastModeData[i + code->m_baseIndex] = registerFile[code->m_loadRegisterIndexs[i]];
                    }
                }
            } else {
                size_t spreadCount = 0;
                for (size_t i = 0; i < code->m_count; i++) {
                    if (LIKELY(code->m_loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                        const Value& element = registerFile[code->m_loadRegisterIndexs[i]];

                        if (element.isObject() && element.asObject()->isSpreadObject()) {
                            SpreadObject* spreadObj = element.asObject()->asSpreadObject();
                            Value iterator = getIterator(state, spreadObj->spreadValue());

                            while (true) {
                                Value next = iteratorStep(state, iterator);
                                if (next.isFalse()) {
                                    spreadCount--;
                                    break;
                                }

                                Value nextValue = iteratorValue(state, next);
                                arr->defineOwnProperty(state, ObjectPropertyName(state, Value(i + spreadCount + code->m_baseIndex)), ObjectPropertyDescriptor(nextValue, ObjectPropertyDescriptor::AllPresent));
                                spreadCount++;
                            }
                        } else {
                            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(i + spreadCount + code->m_baseIndex)), ObjectPropertyDescriptor(element, ObjectPropertyDescriptor::AllPresent));
                        }
                    }
                }
            }
            ADD_PROGRAM_COUNTER(ArrayDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateSpreadObject)
            :
        {
            CreateSpreadObject* code = (CreateSpreadObject*)programCounter;
            registerFile[code->m_registerIndex] = new SpreadObject(state, registerFile[code->m_spreadIndex]);
            ADD_PROGRAM_COUNTER(CreateSpreadObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(NewOperation)
            :
        {
            NewOperation* code = (NewOperation*)programCounter;
            registerFile[code->m_resultIndex] = Object::construct(state, registerFile[code->m_calleeIndex], code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(NewOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryTypeof)
            :
        {
            UnaryTypeof* code = (UnaryTypeof*)programCounter;
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
            ADD_PROGRAM_COUNTER(UnaryTypeof);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetObjectOpcodeSlowCase)
            :
        {
            GetObject* code = (GetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            Object* obj;
            if (LIKELY(willBeObject.isObject())) {
                obj = willBeObject.asObject();
            } else {
                obj = fastToObject(state, willBeObject);
            }
            registerFile[code->m_storeRegisterIndex] = obj->getIndexedProperty(state, property).value(state, willBeObject);
            ADD_PROGRAM_COUNTER(GetObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetObjectOpcodeSlowCase)
            :
        {
            SetObjectOperation* code = (SetObjectOperation*)programCounter;
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
            ADD_PROGRAM_COUNTER(SetObjectOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadByName)
            :
        {
            LoadByName* code = (LoadByName*)programCounter;
            registerFile[code->m_registerIndex] = loadByName(state, state.lexicalEnvironment(), code->m_name);
            ADD_PROGRAM_COUNTER(LoadByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(StoreByName)
            :
        {
            StoreByName* code = (StoreByName*)programCounter;
            storeByName(state, state.lexicalEnvironment(), code->m_name, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(InitializeByName)
            :
        {
            InitializeByName* code = (InitializeByName*)programCounter;
            initializeByName(state, state.lexicalEnvironment(), code->m_name, code->m_isLexicallyDeclaredName, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(InitializeByName);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateFunction)
            :
        {
            CreateFunction* code = (CreateFunction*)programCounter;
            createFunctionOperation(state, code, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(CreateFunction);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateClass)
            :
        {
            CreateClass* code = (CreateClass*)programCounter;
            classOperation(state, code, registerFile);
            ADD_PROGRAM_COUNTER(CreateClass);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SuperReference)
            :
        {
            SuperReference* code = (SuperReference*)programCounter;
            superOperation(state, code, registerFile);
            ADD_PROGRAM_COUNTER(SuperReference);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallSuper)
            :
        {
            CallSuper* code = (CallSuper*)programCounter;
            callSuperOperation(state, code, registerFile);
            ADD_PROGRAM_COUNTER(CallSuper);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateRestElement)
            :
        {
            CreateRestElement* code = (CreateRestElement*)programCounter;
            registerFile[code->m_registerIndex] = createRestElementOperation(state, byteCodeBlock);
            ADD_PROGRAM_COUNTER(CreateRestElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadThisBinding)
            :
        {
            LoadThisBinding* code = (LoadThisBinding*)programCounter;
            EnvironmentRecord* envRec = state.getThisEnvironment();
            ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
            registerFile[code->m_dstIndex] = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state);
            ADD_PROGRAM_COUNTER(LoadThisBinding);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallEvalFunction)
            :
        {
            CallEvalFunction* code = (CallEvalFunction*)programCounter;
            evalOperation(state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(CallEvalFunction);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TryOperation)
            :
        {
            TryOperation* code = (TryOperation*)programCounter;
            programCounter = tryOperation(state, code, state.lexicalEnvironment(), programCounter, byteCodeBlock, registerFile);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TryCatchWithBlockBodyEnd)
            :
        {
            (*(state.rareData()->m_controlFlowRecord))[state.rareData()->m_controlFlowRecord->size() - 1] = nullptr;
            return Value();
        }

        DEFINE_OPCODE(FinallyEnd)
            :
        {
            FinallyEnd* code = (FinallyEnd*)programCounter;
            ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
            state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);
            if (record) {
                if (record->reason() == ControlFlowRecord::NeedsJump) {
                    size_t pos = record->wordValue();
                    record->m_count--;
                    if (record->count() && (record->outerLimitCount() < record->count())) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                        return Value();
                    } else {
                        programCounter = jumpTo(codeBuffer, pos);
                    }
                } else if (record->reason() == ControlFlowRecord::NeedsThrow) {
                    state.context()->throwException(state, record->value());
                } else if (record->reason() == ControlFlowRecord::NeedsReturn) {
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                        return Value();
                    } else {
                        return record->value();
                    }
                }
            } else {
                ADD_PROGRAM_COUNTER(FinallyEnd);
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ThrowOperation)
            :
        {
            ThrowOperation* code = (ThrowOperation*)programCounter;
            state.context()->throwException(state, registerFile[code->m_registerIndex]);
        }

        DEFINE_OPCODE(WithOperation)
            :
        {
            WithOperation* code = (WithOperation*)programCounter;
            size_t newPc = programCounter;
            Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
            Value v = withOperation(state, code, registerFile[code->m_registerIndex].toObject(state), newPc, byteCodeBlock, registerFile, stackStorage);
            if (!v.isEmpty()) {
                return v;
            }
            if (programCounter == newPc) {
                return Value();
            }
            programCounter = newPc;
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpComplexCase)
            :
        {
            JumpComplexCase* code = (JumpComplexCase*)programCounter;
            state.ensureRareData()->m_controlFlowRecord->back() = code->m_controlFlowRecord->clone();
            return Value();
        }

        DEFINE_OPCODE(EnumerateObject)
            :
        {
            EnumerateObject* code = (EnumerateObject*)programCounter;
            auto data = executeEnumerateObject(state, registerFile[code->m_objectRegisterIndex].toObject(state));
            registerFile[code->m_dataRegisterIndex] = Value((PointerValue*)data);
            ADD_PROGRAM_COUNTER(EnumerateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CheckIfKeyIsLast)
            :
        {
            CheckIfKeyIsLast* code = (CheckIfKeyIsLast*)programCounter;
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
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(EnumerateObjectKey)
            :
        {
            EnumerateObjectKey* code = (EnumerateObjectKey*)programCounter;
            EnumerateObjectData* data = (EnumerateObjectData*)registerFile[code->m_dataRegisterIndex].asPointerValue();
            data->m_idx++;
            registerFile[code->m_registerIndex] = Value(data->m_keys[data->m_idx - 1]).toString(state);
            ADD_PROGRAM_COUNTER(EnumerateObjectKey);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetIterator)
            :
        {
            GetIterator* code = (GetIterator*)programCounter;
            Value obj = registerFile[code->m_objectRegisterIndex];
            registerFile[code->m_registerIndex] = getIterator(state, obj);
            ADD_PROGRAM_COUNTER(GetIterator);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(IteratorStep)
            :
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
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadRegexp)
            :
        {
            LoadRegexp* code = (LoadRegexp*)programCounter;
            auto reg = new RegExpObject(state, code->m_body, code->m_option);
            registerFile[code->m_registerIndex] = reg;
            ADD_PROGRAM_COUNTER(LoadRegexp);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryDelete)
            :
        {
            UnaryDelete* code = (UnaryDelete*)programCounter;
            deleteOperation(state, state.lexicalEnvironment(), code, registerFile);
            ADD_PROGRAM_COUNTER(UnaryDelete);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(TemplateOperation)
            :
        {
            TemplateOperation* code = (TemplateOperation*)programCounter;
            templateOperation(state, state.lexicalEnvironment(), code, registerFile);
            ADD_PROGRAM_COUNTER(TemplateOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryInOperation)
            :
        {
            BinaryInOperation* code = (BinaryInOperation*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            bool result = binaryInOperation(state, left, right);
            registerFile[code->m_dstIndex] = Value(result);
            ADD_PROGRAM_COUNTER(BinaryInOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryInstanceOfOperation)
            :
        {
            BinaryInstanceOfOperation* code = (BinaryInstanceOfOperation*)programCounter;
            registerFile[code->m_dstIndex] = instanceOfOperation(state, registerFile[code->m_srcIndex0], registerFile[code->m_srcIndex1]);
            ADD_PROGRAM_COUNTER(BinaryInstanceOfOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineGetter)
            :
        {
            ObjectDefineGetter* code = (ObjectDefineGetter*)programCounter;
            defineObjectGetter(state, code, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineGetter);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineSetter)
            :
        {
            ObjectDefineSetter* code = (ObjectDefineSetter*)programCounter;
            defineObjectSetter(state, code, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineGetter);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CallFunctionInWithScope)
            :
        {
            CallFunctionInWithScope* code = (CallFunctionInWithScope*)programCounter;
            registerFile[code->m_resultIndex] = callFunctionInWithScope(state, code, state.lexicalEnvironment(), &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(CallFunctionInWithScope);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ReturnFunctionSlowCase)
            :
        {
            ReturnFunctionSlowCase* code = (ReturnFunctionSlowCase*)programCounter;
            Value ret;
            if (code->m_registerIndex != REGISTER_LIMIT) {
                ret = registerFile[code->m_registerIndex];
            }
            if (UNLIKELY(state.rareData() != nullptr) && state.rareData()->m_controlFlowRecord && state.rareData()->m_controlFlowRecord->size()) {
                state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, ret, state.rareData()->m_controlFlowRecord->size());
            }
            return ret;
        }

        DEFINE_OPCODE(ThrowStaticErrorOperation)
            :
        {
            ThrowStaticErrorOperation* code = (ThrowStaticErrorOperation*)programCounter;
            ErrorObject::throwBuiltinError(state, (ErrorObject::Code)code->m_errorKind, code->m_errorMessage, code->m_templateDataString);
        }

        DEFINE_OPCODE(CallFunctionWithSpreadElement)
            :
        {
            CallFunctionWithSpreadElement* code = (CallFunctionWithSpreadElement*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            const Value& receiver = code->m_receiverIndex == REGISTER_LIMIT ? Value() : registerFile[code->m_receiverIndex];
            ValueVector spreadArgs;
            spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
            registerFile[code->m_resultIndex] = Object::call(state, callee, receiver, spreadArgs.size(), spreadArgs.data());
            ADD_PROGRAM_COUNTER(CallFunctionWithSpreadElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(NewOperationWithSpreadElement)
            :
        {
            NewOperationWithSpreadElement* code = (NewOperationWithSpreadElement*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            ValueVector spreadArgs;
            spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
            registerFile[code->m_resultIndex] = Object::construct(state, registerFile[code->m_calleeIndex], spreadArgs.size(), spreadArgs.data());
            ADD_PROGRAM_COUNTER(NewOperationWithSpreadElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BindingRestElement)
            :
        {
            BindingRestElement* code = (BindingRestElement*)programCounter;

            ArrayObject* array = new ArrayObject(state, false);
            const Value& iterator = registerFile[code->m_iterIndex];

            size_t i = 0;
            while (true) {
                Value next = iteratorStep(state, iterator);
                if (next.isFalse()) {
                    break;
                }

                array->setIndexedProperty(state, Value(i++), iteratorValue(state, next));
            }

            registerFile[code->m_dstIndex] = array;

            ADD_PROGRAM_COUNTER(BindingRestElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GeneratorComplete)
            :
        {
            GeneratorComplete* code = (GeneratorComplete*)programCounter;
            state.generatorTarget()->asGeneratorObject()->setState((GeneratorState)(GeneratorState::CompletedReturn + !!code->m_isThrow));
            ADD_PROGRAM_COUNTER(GeneratorComplete);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(Yield)
            :
        {
            Yield* code = (Yield*)programCounter;
            GeneratorObject* gen = state.generatorTarget()->asGeneratorObject();

            gen->setState(GeneratorState::SuspendedYield);
            ADD_PROGRAM_COUNTER(Yield);
            gen->setByteCodePosition((char*)programCounter - codeBuffer);
            gen->setResumeValueIdx(code->m_dstIdx);
            return code->m_yieldIdx == REGISTER_LIMIT ? Value() : registerFile[code->m_yieldIdx];
        }

        DEFINE_OPCODE(YieldDelegate)
            :
        {
            Value result = yieldDelegateOperation(state, registerFile, programCounter, codeBuffer);

            if (result.isEmpty()) {
                NEXT_INSTRUCTION();
            }

            return result;
        }

        DEFINE_OPCODE(BlockOperation)
            :
        {
            BlockOperation* code = (BlockOperation*)programCounter;
            size_t newPc = programCounter;
            Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
            Value v = blockOperation(state, code, newPc, byteCodeBlock, registerFile, stackStorage);
            if (!v.isEmpty()) {
                return v;
            }
            if (programCounter == newPc) {
                return Value();
            }
            programCounter = newPc;
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(EnsureArgumentsObject)
            :
        {
            ensureArgumentsObjectOperation(state, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(EnsureArgumentsObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(End)
            :
        {
            return registerFile[0];
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

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-instanceofoperator
NEVER_INLINE Value ByteCodeInterpreter::instanceOfOperation(ExecutionState& state, const Value& left, const Value& right)
{
    // If Type(C) is not Object, throw a TypeError exception.
    if (!right.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    // Let instOfHandler be GetMethod(C,@@hasInstance).
    Value instOfHandler = Object::getMethod(state, right, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().hasInstance));
    // If instOfHandler is not undefined, then
    if (!instOfHandler.isUndefined()) {
        // Return ToBoolean(Call(instOfHandler, C, «O»)).
        Value arg[1] = { left };
        return Value(Object::call(state, instOfHandler, right, 1, arg).toBoolean(state));
    }

    // If IsCallable(C) is false, throw a TypeError exception.
    if (!right.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    // Return OrdinaryHasInstance(C, O).
    return Value(Object::hasInstance(state, right, left));
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

    size_t ownKeyCount = 0;
    bool shouldSearchProto = false;

    target.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        if (desc.isEnumerable()) {
            size_t* ownKeyCount = (size_t*)data;
            (*ownKeyCount)++;
        }
        return true;
    },
                                   &ownKeyCount);
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
    struct EData {
        std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_allocator<String*>>* keyStringSet;
        EnumerateObjectData* data;
        Object* obj;
        size_t* idx;
    } eData;

    eData.data = data;
    eData.keyStringSet = &keyStringSet;
    eData.obj = obj;

    if (shouldSearchProto) {
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
        size_t idx = 0;
        eData.idx = &idx;
        data->m_keys.resizeWithUninitializedValues(ownKeyCount);
        target.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
            if (desc.isEnumerable()) {
                EData* eData = (EData*)data;
                eData->data->m_keys[(*eData->idx)++] = name.toPlainValue(state);
            }
            return true;
        },
                                       &eData);
        ASSERT(ownKeyCount == idx);
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
    }

    return go->getOwnPropertyUtilForObject(state, idx, go);
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

NEVER_INLINE size_t ByteCodeInterpreter::tryOperation(ExecutionState& state, TryOperation* code, LexicalEnvironment* env, size_t programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    char* codeBuffer = byteCodeBlock->m_code.data();
    bool oldInTryStatement = state.m_inTryStatement;
    try {
        state.m_inTryStatement = true;
        if (!state.ensureRareData()->m_controlFlowRecord) {
            state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        size_t newPc = programCounter + sizeof(TryOperation);
        clearStack<386>();
        interpret(state, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);
        programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
        state.m_inTryStatement = oldInTryStatement;
    } catch (const Value& val) {
        state.m_inTryStatement = oldInTryStatement;
        state.context()->m_sandBoxStack.back()->fillStackDataIntoErrorObject(val);

#ifndef NDEBUG
        if (getenv("DUMP_ERROR_IN_TRY_CATCH") && strlen(getenv("DUMP_ERROR_IN_TRY_CATCH"))) {
            ErrorObject::StackTraceData* data = ErrorObject::StackTraceData::create(state.context()->m_sandBoxStack.back());
            StringBuilder builder;
            builder.appendString("Caught error in try-catch block\n");
            data->buildStackTrace(state.context(), builder);
            ESCARGOT_LOG_ERROR("%s\n", builder.finalize()->toUTF8StringData().data());
        }
#endif

        state.context()->m_sandBoxStack.back()->m_stackTraceData.clear();
        if (!code->m_hasCatch) {
            state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
            programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
        } else {
            registerFile[code->m_catchedValueRegisterIndex] = val;
            try {
                interpret(state, byteCodeBlock, code->m_catchPosition, registerFile);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            } catch (const Value& val) {
                state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            }
        }
    }
    return programCounter;
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
        constructor = new ScriptClassConstructorFunctionObject(state, code->m_codeBlock, state.lexicalEnvironment(), proto);
    } else {
        if (!heritagePresent) {
            Value argv[] = { String::emptyString, String::emptyString };
            auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false);
            functionSource.codeBlock->setAsClassConstructor();
            constructor = new ScriptClassConstructorFunctionObject(state, functionSource.codeBlock, functionSource.outerEnvironment, proto);
        } else {
            Value argv[] = { new ASCIIString("...args"), new ASCIIString("super(...args)") };
            auto functionSource = FunctionObject::createFunctionSourceFromScriptSource(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false);
            functionSource.codeBlock->setAsClassConstructor();
            functionSource.codeBlock->setAsDerivedClassConstructor();
            constructor = new ScriptClassConstructorFunctionObject(state, functionSource.codeBlock, functionSource.outerEnvironment, proto);
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

NEVER_INLINE Value ByteCodeInterpreter::withOperation(ExecutionState& state, WithOperation* code, Object* obj, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile, Value* stackStorage)
{
    LexicalEnvironment* env = state.lexicalEnvironment();
    if (!state.ensureRareData()->m_controlFlowRecord) {
        state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
    }
    state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
    size_t newPc = programCounter + sizeof(WithOperation);
    char* codeBuffer = byteCodeBlock->m_code.data();

    // setup new env
    EnvironmentRecord* newRecord = new ObjectEnvironmentRecord(obj);
    ASSERT(env->isAllocatedOnHeap());
    LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
    ASSERT(newEnv->isAllocatedOnHeap());
    ExecutionState newState(&state, newEnv, state.inStrictMode());
    newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;

    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);

    ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
    state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);

    if (record) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count() && (record->outerLimitCount() < record->count())) {
                state.rareData()->m_controlFlowRecord->back() = record;
            } else {
                programCounter = jumpTo(codeBuffer, pos);
            }
            return Value(Value::EmptyValue);
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state.rareData()->m_controlFlowRecord->back() = record;
            }
            return record->value();
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_withEndPostion);
    }
    return Value(Value::EmptyValue);
}

NEVER_INLINE Value ByteCodeInterpreter::blockOperation(ExecutionState& state, BlockOperation* code, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile, Value* stackStorage)
{
    if (!state.ensureRareData()->m_controlFlowRecord) {
        state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
    }
    state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
    size_t newPc = programCounter + sizeof(BlockOperation);
    char* codeBuffer = byteCodeBlock->m_code.data();

    ASSERT(code->m_blockInfo->m_shouldAllocateEnvironment);

    // setup new env
    EnvironmentRecord* newRecord;
    bool shouldUseIndexedStorage = byteCodeBlock->m_codeBlock->canUseIndexedVariableStorage();

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

    LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, state.lexicalEnvironment());
    ASSERT(newEnv->isAllocatedOnHeap());
    ExecutionState newState(&state, newEnv, state.inStrictMode());
    newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;

    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);

    ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
    state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);

    if (record != nullptr) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count() && (record->outerLimitCount() < record->count())) {
                state.rareData()->m_controlFlowRecord->back() = record;
            } else {
                programCounter = jumpTo(codeBuffer, pos);
            }
            return Value(Value::EmptyValue);
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state.rareData()->m_controlFlowRecord->back() = record;
            }
            return record->value();
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_blockEndPosition);
    }
    return Value(Value::EmptyValue);
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

NEVER_INLINE Value ByteCodeInterpreter::callFunctionInWithScope(ExecutionState& state, CallFunctionInWithScope* code, LexicalEnvironment* env, Value* argv)
{
    const AtomicString& calleeName = code->m_calleeName;
    // NOTE: record for with scope
    Object* receiverObj = NULL;
    Value callee;
    EnvironmentRecord* bindedRecord = getBindedEnvironmentRecordByName(state, env, calleeName, callee);
    if (!bindedRecord)
        callee = Value();

    if (bindedRecord && bindedRecord->isObjectEnvironmentRecord())
        receiverObj = bindedRecord->asObjectEnvironmentRecord()->bindingObject();
    else
        receiverObj = state.context()->globalObject();

    if (code->m_hasSpreadElement) {
        ValueVector spreadArgs;
        spreadFunctionArguments(state, argv, code->m_argumentCount, spreadArgs);
        return Object::call(state, callee, receiverObj, spreadArgs.size(), spreadArgs.data());
    }
    return Object::call(state, callee, receiverObj, code->m_argumentCount, argv);
}

void ByteCodeInterpreter::spreadFunctionArguments(ExecutionState& state, const Value* argv, const size_t argc, ValueVector& argVector)
{
    for (size_t i = 0; i < argc; i++) {
        Value arg = argv[i];
        if (arg.isObject() && arg.asObject()->isSpreadObject()) {
            SpreadObject* spreadObj = arg.asObject()->asSpreadObject();
            Value iterator = getIterator(state, spreadObj->spreadValue());

            while (true) {
                Value next = iteratorStep(state, iterator);
                if (next.isFalse()) {
                    break;
                }

                argVector.push_back(iteratorValue(state, next));
            }
        } else {
            argVector.push_back(arg);
        }
    }
}

Value ByteCodeInterpreter::yieldDelegateOperation(ExecutionState& state, Value* registerFile, size_t& programCounter, char* codeBuffer)
{
    ASSERT(registerFile != nullptr);
    ASSERT(codeBuffer != nullptr);

    YieldDelegate* code = (YieldDelegate*)programCounter;
    const Value iterator = registerFile[code->m_iterIdx].toObject(state);
    GeneratorState resultState = GeneratorState::SuspendedYield;
    Value nextResult;
    bool done = true;
    Value nextValue;
    try {
        nextResult = iteratorNext(state, iterator);
        if (iterator.asObject()->isGeneratorObject()) {
            resultState = iterator.asObject()->asGeneratorObject()->state();
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
            iteratorClose(state, iterator);
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
    GeneratorObject* gen = state.generatorTarget()->asGeneratorObject();

    gen->setState(GeneratorState::SuspendedYield);
    ADD_PROGRAM_COUNTER(YieldDelegate);

    gen->setByteCodePosition((char*)programCounter - codeBuffer);
    return nextValue;
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectGetter(ExecutionState& state, ObjectDefineGetter* code, Value* registerFile)
{
    // FIXME: FunctionObject
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    String* pName = registerFile[code->m_objectPropertyNameRegisterIndex].toString(state);
    StringBuilder builder;
    builder.appendString("get ");
    builder.appendString(pName);
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(builder.finalize()));
    JSGetterSetter gs(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
    Object* object = registerFile[code->m_objectRegisterIndex].toObject(state);
    object->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, pName), desc);
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectSetter(ExecutionState& state, ObjectDefineSetter* code, Value* registerFile)
{
    // FIXME: FunctionObject
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    String* pName = registerFile[code->m_objectPropertyNameRegisterIndex].toString(state);
    StringBuilder builder;
    builder.appendString("set ");
    builder.appendString(pName);
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(builder.finalize()));
    JSGetterSetter gs(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
    ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
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
    bool isMapped = !functionObject->codeBlock()->hasArgumentInitializers() && !functionObject->codeBlock()->isStrict();
    r->functionObject()->asScriptFunctionObject()->generateArgumentsObject(state, state.argc(), state.argv(), r, registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize, isMapped);
}
}
