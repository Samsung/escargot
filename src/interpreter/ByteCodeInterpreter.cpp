/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/FunctionObject.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"
#include "runtime/GlobalObject.h"
#include "runtime/StringObject.h"
#include "runtime/NumberObject.h"
#include "runtime/BigIntObject.h"
#include "runtime/EnumerateObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/VMInstance.h"
#include "runtime/IteratorObject.h"
#include "runtime/GeneratorObject.h"
#include "runtime/ModuleNamespaceObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ScriptArrowFunctionObject.h"
#include "runtime/ScriptVirtualArrowFunctionObject.h"
#include "runtime/ScriptClassConstructorFunctionObject.h"
#include "runtime/ScriptClassMethodFunctionObject.h"
#include "runtime/ScriptGeneratorFunctionObject.h"
#include "runtime/ScriptAsyncFunctionObject.h"
#include "runtime/ScriptAsyncGeneratorFunctionObject.h"
#include "parser/ScriptParser.h"
#include "CheckedArithmetic.h"

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

template <typename T>
class ExecutionStateVariableChanger {
public:
    ExecutionStateVariableChanger(ExecutionState& state, T changer)
        : m_state(state)
        , m_changer(changer)
    {
        m_changer(m_state, true);
    }
    ~ExecutionStateVariableChanger()
    {
        m_changer(m_state, false);
    }

private:
    ExecutionState& m_state;
    T m_changer;
};

Value ByteCodeInterpreter::interpret(ExecutionState* state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile)
{
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
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

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
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
            ASSERT(code->m_registerIndex1 < (byteCodeBlock->m_requiredOperandRegisterNumber + byteCodeBlock->m_codeBlock->totalStackAllocatedVariableSize() + 1));
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
            GlobalObject* globalObject = ctx->globalObject();
            auto slot = code->m_slot;
            auto idx = slot->m_lexicalIndexCache;
            bool isCacheWork = false;

            if (LIKELY(idx != std::numeric_limits<size_t>::max())) {
                if (LIKELY(ctx->globalDeclarativeStorage()->size() == slot->m_lexicalIndexCache && globalObject->structure() == slot->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= slot->m_cachedAddress);
                    ASSERT(slot->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    registerFile[code->m_registerIndex] = *((ObjectPropertyValue*)slot->m_cachedAddress);
                    isCacheWork = true;
                } else if (slot->m_cachedStructure == nullptr) {
                    const EncodedValueVectorElement& val = ctx->globalDeclarativeStorage()->at(idx);
                    isCacheWork = true;
                    if (UNLIKELY(val.isEmpty())) {
                        ErrorObject::throwBuiltinError(*state, ErrorObject::ReferenceError, ctx->globalDeclarativeRecord()->at(idx).m_name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
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
            GlobalObject* globalObject = ctx->globalObject();
            auto slot = code->m_slot;
            auto idx = slot->m_lexicalIndexCache;

            bool isCacheWork = false;
            if (LIKELY(idx != std::numeric_limits<size_t>::max())) {
                if (LIKELY(ctx->globalDeclarativeStorage()->size() == slot->m_lexicalIndexCache && globalObject->structure() == slot->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= slot->m_cachedAddress);
                    ASSERT(slot->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    *((ObjectPropertyValue*)slot->m_cachedAddress) = registerFile[code->m_registerIndex];
                    isCacheWork = true;
                } else if (slot->m_cachedStructure == nullptr) {
                    isCacheWork = true;
                    const auto& record = ctx->globalDeclarativeRecord()->at(idx);
                    auto& storage = ctx->globalDeclarativeStorage()->at(idx);
                    if (UNLIKELY(storage.isEmpty())) {
                        ErrorObject::throwBuiltinError(*state, ErrorObject::ReferenceError, record.m_name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
                    }
                    if (UNLIKELY(!record.m_isMutable)) {
                        ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, record.m_name.string(), false, String::emptyString, ErrorObject::Messages::AssignmentToConstantVariable);
                    }
                    storage = registerFile[code->m_registerIndex];
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
            Value& ret = registerFile[code->m_dstIndex];
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
            ADD_PROGRAM_COUNTER(BinaryPlus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMinus)
            :
        {
            BinaryMinus* code = (BinaryMinus*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            Value& ret = registerFile[code->m_dstIndex];
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
            } else if (LIKELY(left.isNumber() && right.isNumber())) {
                ret = Value(left.asNumber() - right.asNumber());
            } else {
                ret = minusSlowCase(*state, left, right);
            }
            ADD_PROGRAM_COUNTER(BinaryMinus);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryMultiply)
            :
        {
            BinaryMultiply* code = (BinaryMultiply*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            Value& ret = registerFile[code->m_dstIndex];
            if (left.isInt32() && right.isInt32()) {
                int32_t a = left.asInt32();
                int32_t b = right.asInt32();
                if (UNLIKELY((!a || !b) && (a >> 31 || b >> 31))) { // -1 * 0 should be treated as -0, not +0
                    ret = Value(left.asNumber() * right.asNumber());
                } else {
                    int32_t c;
                    bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::multiply(a, b, c);
                    if (LIKELY(result)) {
                        ret = Value(c);
                    } else {
                        ret = Value(Value::EncodeAsDouble, a * (double)b);
                    }
                }
            } else if (LIKELY(left.isNumber() && right.isNumber())) {
                ret = Value(left.asNumber() * right.asNumber());
            } else {
                ret = multiplySlowCase(*state, left, right);
            }
            ADD_PROGRAM_COUNTER(BinaryMultiply);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryDivision)
            :
        {
            BinaryDivision* code = (BinaryDivision*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            Value& ret = registerFile[code->m_dstIndex];
            if (LIKELY(left.isNumber() && right.isNumber())) {
                ret = Value(left.asNumber() / right.asNumber());
            } else {
                ret = divisionSlowCase(*state, left, right);
            }
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
            registerFile[code->m_dstIndex] = Value(abstractLeftIsLessThanRight(*state, left, right, false));
            ADD_PROGRAM_COUNTER(BinaryLessThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLessThanOrEqual)
            :
        {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractLeftIsLessThanEqualRight(*state, left, right, false));
            ADD_PROGRAM_COUNTER(BinaryLessThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThan)
            :
        {
            BinaryGreaterThan* code = (BinaryGreaterThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractLeftIsLessThanRight(*state, right, left, true));
            ADD_PROGRAM_COUNTER(BinaryGreaterThan);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryGreaterThanOrEqual)
            :
        {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractLeftIsLessThanEqualRight(*state, right, left, true));
            ADD_PROGRAM_COUNTER(BinaryGreaterThanOrEqual);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ToNumericIncrement)
            :
        {
            ToNumericIncrement* code = (ToNumericIncrement*)programCounter;
            registerFile[code->m_dstIndex] = Value(registerFile[code->m_srcIndex].toNumeric(*state).first);
            registerFile[code->m_storeIndex] = incrementOperation(*state, registerFile[code->m_dstIndex]);
            ADD_PROGRAM_COUNTER(ToNumericIncrement);
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

        DEFINE_OPCODE(ToNumericDecrement)
            :
        {
            ToNumericDecrement* code = (ToNumericDecrement*)programCounter;
            registerFile[code->m_dstIndex] = Value(registerFile[code->m_srcIndex].toNumeric(*state).first);
            registerFile[code->m_storeIndex] = decrementOperation(*state, registerFile[code->m_dstIndex]);
            ADD_PROGRAM_COUNTER(ToNumericDecrement);
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
            Object* obj;
            if (LIKELY(willBeObject.isObject())) {
                obj = willBeObject.asObject();
                if (LIKELY(obj->hasArrayObjectTag())) {
                    ArrayObject* arr = reinterpret_cast<ArrayObject*>(obj);
                    if (LIKELY(arr->isFastModeArray())) {
                        uint32_t idx = property.tryToUseAsIndexProperty(*state);
                        if (LIKELY(idx < arr->arrayLength(*state))) {
                            registerFile[code->m_storeRegisterIndex] = arr->m_fastModeData[idx].toValue<true>();
                            ADD_PROGRAM_COUNTER(GetObject);
                            NEXT_INSTRUCTION();
                        }
                    }
                } else {
                    registerFile[code->m_storeRegisterIndex] = obj->getIndexedPropertyValue(*state, property, willBeObject);
                    ADD_PROGRAM_COUNTER(GetObject);
                    NEXT_INSTRUCTION();
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
            if (LIKELY(willBeObject.isObject() && (willBeObject.asPointerValue())->hasArrayObjectTag())) {
                ArrayObject* arr = willBeObject.asObject()->asArrayObject();
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx = property.tryToUseAsIndexProperty(*state);
                    if (LIKELY(idx < arr->arrayLength(*state))) {
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
            getObjectPrecomputedCaseOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(GetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(SetObjectPreComputedCase)
            :
        {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)programCounter;
            setObjectPreComputedCaseOperation(*state, registerFile[code->m_objectRegisterIndex], registerFile[code->m_loadRegisterIndex], code, byteCodeBlock);
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

        DEFINE_OPCODE(JumpIfNotFulfilled)
            :
        {
            JumpIfNotFulfilled* code = (JumpIfNotFulfilled*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            const Value& left = registerFile[code->m_leftIndex];
            const Value& right = registerFile[code->m_rightIndex];
            bool result = code->m_containEqual ? abstractLeftIsLessThanEqualRight(*state, left, right, code->m_switched) : abstractLeftIsLessThanRight(*state, left, right, code->m_switched);

            // Jump if the condition is NOT fulfilled
            if (result) {
                ADD_PROGRAM_COUNTER(JumpIfNotFulfilled);
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
            bool result = code->m_isStrict ? left.equalsTo(*state, right) : left.abstractEqualsTo(*state, right);

            if (result ^ code->m_shouldNegate) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfEqual);
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

        DEFINE_OPCODE(JumpIfUndefinedOrNull)
            :
        {
            JumpIfUndefinedOrNull* code = (JumpIfUndefinedOrNull*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            bool result = registerFile[code->m_registerIndex].isUndefinedOrNull();

            if (result ^ code->m_shouldNegate) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfUndefinedOrNull);
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
                ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
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
                ErrorObject::throwBuiltinError(*state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
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

        DEFINE_OPCODE(UnaryMinus)
            :
        {
            UnaryMinus* code = (UnaryMinus*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            if (UNLIKELY(val.isPointerValue())) {
                registerFile[code->m_dstIndex] = unaryMinusSlowCase(*state, val);
            } else {
                registerFile[code->m_dstIndex] = Value(-val.toNumber(*state));
            }
            ADD_PROGRAM_COUNTER(UnaryMinus);
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
            if (left.isInt32() && right.isInt32()) {
                registerFile[code->m_dstIndex] = Value(left.asInt32() & right.asInt32());
            } else {
                registerFile[code->m_dstIndex] = bitwiseOperationSlowCase(*state, left, right, BitwiseOperationKind::And);
            }
            ADD_PROGRAM_COUNTER(BinaryBitwiseAnd);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseOr)
            :
        {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            if (left.isInt32() && right.isInt32()) {
                registerFile[code->m_dstIndex] = Value(left.asInt32() | right.asInt32());
            } else {
                registerFile[code->m_dstIndex] = bitwiseOperationSlowCase(*state, left, right, BitwiseOperationKind::Or);
            }
            ADD_PROGRAM_COUNTER(BinaryBitwiseOr);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryBitwiseXor)
            :
        {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            if (left.isInt32() && right.isInt32()) {
                registerFile[code->m_dstIndex] = Value(left.asInt32() ^ right.asInt32());
            } else {
                registerFile[code->m_dstIndex] = bitwiseOperationSlowCase(*state, left, right, BitwiseOperationKind::Xor);
            }
            ADD_PROGRAM_COUNTER(BinaryBitwiseXor);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryLeftShift)
            :
        {
            BinaryLeftShift* code = (BinaryLeftShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            if (left.isInt32() && right.isInt32()) {
                int32_t lnum = left.asInt32();
                int32_t rnum = right.asInt32();
                lnum <<= ((unsigned int)rnum) & 0x1F;
                registerFile[code->m_dstIndex] = Value(lnum);
            } else {
                registerFile[code->m_dstIndex] = shiftOperationSlowCase(*state, left, right, ShiftOperationKind::Left);
            }
            ADD_PROGRAM_COUNTER(BinaryLeftShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinarySignedRightShift)
            :
        {
            BinarySignedRightShift* code = (BinarySignedRightShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            if (left.isInt32() && right.isInt32()) {
                int32_t lnum = left.asInt32();
                int32_t rnum = right.asInt32();
                lnum >>= ((unsigned int)rnum) & 0x1F;
                registerFile[code->m_dstIndex] = Value(lnum);
            } else {
                registerFile[code->m_dstIndex] = shiftOperationSlowCase(*state, left, right, ShiftOperationKind::SignedRight);
            }
            ADD_PROGRAM_COUNTER(BinarySignedRightShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryUnsignedRightShift)
            :
        {
            BinaryUnsignedRightShift* code = (BinaryUnsignedRightShift*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            if (left.isUInt32() && right.isUInt32()) {
                uint32_t lnum = left.asUInt32();
                uint32_t rnum = right.asUInt32();
                lnum = (lnum) >> ((rnum)&0x1F);
                registerFile[code->m_dstIndex] = Value(lnum);
            } else {
                registerFile[code->m_dstIndex] = shiftOperationSlowCase(*state, left, right, ShiftOperationKind::UnsignedRight);
            }
            ADD_PROGRAM_COUNTER(BinaryUnsignedRightShift);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BinaryExponentiation)
            :
        {
            BinaryExponentiation* code = (BinaryExponentiation*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = exponentialOperation(*state, left, right);
            ADD_PROGRAM_COUNTER(BinaryExponentiation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryBitwiseNot)
            :
        {
            UnaryBitwiseNot* code = (UnaryBitwiseNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            if (val.isInt32()) {
                registerFile[code->m_dstIndex] = Value(~val.asInt32());
            } else {
                registerFile[code->m_dstIndex] = bitwiseNotOperationSlowCase(*state, val);
            }
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
#ifdef ESCARGOT_DEBUGGER
            Debugger::updateStopState(state->context()->debugger(), state, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */

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

        DEFINE_OPCODE(BindingCalleeIntoRegister)
            :
        {
            BindingCalleeIntoRegister* code = (BindingCalleeIntoRegister*)programCounter;
            registerFile[byteCodeBlock->m_requiredOperandRegisterNumber + 1] = state->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
            ADD_PROGRAM_COUNTER(BindingCalleeIntoRegister);
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
            objectDefineOwnPropertyWithNameOperation(*state, code, byteCodeBlock, registerFile);
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
            registerFile[code->m_resultIndex] = constructOperation(*state, registerFile[code->m_calleeIndex], code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
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
#if defined(ESCARGOT_SMALL_CONFIG)
            registerFile[code->m_registerIndex].asObject()->markThisObjectDontNeedStructureTransitionTable();
#endif
            ADD_PROGRAM_COUNTER(CreateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CreateArray)
            :
        {
            CreateArray* code = (CreateArray*)programCounter;
            registerFile[code->m_registerIndex] = new ArrayObject(*state, (uint64_t)code->m_length);
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

        DEFINE_OPCODE(InitializeClass)
            :
        {
            InitializeClass* code = (InitializeClass*)programCounter;
            initializeClassOperation(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(InitializeClass);
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

        DEFINE_OPCODE(ComplexSetObjectOperation)
            :
        {
            ComplexSetObjectOperation* code = (ComplexSetObjectOperation*)programCounter;
            complexSetObjectOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(ComplexSetObjectOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ComplexGetObjectOperation)
            :
        {
            ComplexGetObjectOperation* code = (ComplexGetObjectOperation*)programCounter;
            complexGetObjectOperation(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(ComplexGetObjectOperation);
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

        DEFINE_OPCODE(TryOperation)
            :
        {
            Value v = tryOperation(state, programCounter, byteCodeBlock, registerFile);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CloseLexicalEnvironment)
            :
        {
            (*(state->rareData()->m_controlFlowRecord))[state->rareData()->m_controlFlowRecord->size() - 1] = nullptr;
            return Value(Value::EmptyValue);
        }

        DEFINE_OPCODE(ThrowOperation)
            :
        {
            ThrowOperation* code = (ThrowOperation*)programCounter;
            state->context()->throwException(*state, registerFile[code->m_registerIndex]);
        }

        DEFINE_OPCODE(OpenLexicalEnvironment)
            :
        {
            Value v = openLexicalEnvironment(state, programCounter, byteCodeBlock, registerFile);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(JumpComplexCase)
            :
        {
            JumpComplexCase* code = (JumpComplexCase*)programCounter;
            state->ensureRareData()->m_controlFlowRecord->back() = byteCodeBlock->m_jumpFlowRecordData[code->m_recordIndex].createControlFlowRecord();
            return Value(Value::EmptyValue);
        }

        DEFINE_OPCODE(CreateEnumerateObject)
            :
        {
            CreateEnumerateObject* code = (CreateEnumerateObject*)programCounter;
            createEnumerateObject(*state, code, registerFile);
            ADD_PROGRAM_COUNTER(CreateEnumerateObject);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(CheckLastEnumerateKey)
            :
        {
            CheckLastEnumerateKey* code = (CheckLastEnumerateKey*)programCounter;
            checkLastEnumerateKey(*state, code, codeBuffer, programCounter, registerFile);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetEnumerateKey)
            :
        {
            GetEnumerateKey* code = (GetEnumerateKey*)programCounter;
            EnumerateObject* data = (EnumerateObject*)registerFile[code->m_dataRegisterIndex].asPointerValue();
            registerFile[code->m_registerIndex] = Value(data->m_keys[data->m_index++]);
            ADD_PROGRAM_COUNTER(GetEnumerateKey);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(MarkEnumerateKey)
            :
        {
            MarkEnumerateKey* code = (MarkEnumerateKey*)programCounter;
            markEnumerateKey(*state, code, registerFile);

            ADD_PROGRAM_COUNTER(MarkEnumerateKey);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(IteratorOperation)
            :
        {
            IteratorOperation* code = (IteratorOperation*)programCounter;
            iteratorOperation(*state, programCounter, registerFile, codeBuffer);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(GetMethod)
            :
        {
            GetMethod* code = (GetMethod*)programCounter;
            getMethodOperation(*state, programCounter, registerFile);
            ADD_PROGRAM_COUNTER(GetMethod);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(LoadRegExp)
            :
        {
            LoadRegExp* code = (LoadRegExp*)programCounter;
            registerFile[code->m_registerIndex] = new RegExpObject(*state, code->m_body, code->m_option);
            ADD_PROGRAM_COUNTER(LoadRegExp);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(UnaryDelete)
            :
        {
            UnaryDelete* code = (UnaryDelete*)programCounter;
            deleteOperation(*state, state->lexicalEnvironment(), code, registerFile, byteCodeBlock);
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

        DEFINE_OPCODE(MetaPropertyOperation)
            :
        {
            MetaPropertyOperation* code = (MetaPropertyOperation*)programCounter;
            metaPropertyOperation(*state, code, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(MetaPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ObjectDefineGetterSetter)
            :
        {
            ObjectDefineGetterSetter* code = (ObjectDefineGetterSetter*)programCounter;
            defineObjectGetterSetter(*state, code, byteCodeBlock, registerFile);
            ADD_PROGRAM_COUNTER(ObjectDefineGetterSetter);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ReturnFunctionSlowCase)
            :
        {
            ReturnFunctionSlowCase* code = (ReturnFunctionSlowCase*)programCounter;
            if (UNLIKELY(state->rareData() != nullptr) && state->rareData()->m_controlFlowRecord && state->rareData()->m_controlFlowRecord->size()) {
                state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, registerFile[code->m_registerIndex], state->rareData()->m_controlFlowRecord->size());
            }
            return Value(Value::EmptyValue);
        }

        DEFINE_OPCODE(CallFunctionComplexCase)
            :
        {
            CallFunctionComplexCase* code = (CallFunctionComplexCase*)programCounter;
            callFunctionComplexCase(*state, code, registerFile, byteCodeBlock);
            ADD_PROGRAM_COUNTER(CallFunctionComplexCase);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ThrowStaticErrorOperation)
            :
        {
            ThrowStaticErrorOperation* code = (ThrowStaticErrorOperation*)programCounter;
            ErrorObject::throwBuiltinError(*state, (ErrorObject::Code)code->m_errorKind, code->m_errorMessage, code->m_templateDataString);
        }

        DEFINE_OPCODE(NewOperationWithSpreadElement)
            :
        {
            NewOperationWithSpreadElement* code = (NewOperationWithSpreadElement*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];

            {
                ValueVector spreadArgs;
                spreadFunctionArguments(*state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
                registerFile[code->m_resultIndex] = constructOperation(*state, registerFile[code->m_calleeIndex], spreadArgs.size(), spreadArgs.data());
            }

            ADD_PROGRAM_COUNTER(NewOperationWithSpreadElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BindingRestElement)
            :
        {
            BindingRestElement* code = (BindingRestElement*)programCounter;

            Object* result;
            const Value& iterOrEnum = registerFile[code->m_iterOrEnumIndex];

            if (UNLIKELY(iterOrEnum.asPointerValue()->isEnumerateObject())) {
                ASSERT(iterOrEnum.asPointerValue()->isEnumerateObject());
                EnumerateObject* enumObj = (EnumerateObject*)iterOrEnum.asPointerValue();
                result = new Object(*state);
                enumObj->fillRestElement(*state, result);
            } else {
                result = restBindOperation(*state, iterOrEnum.asPointerValue()->asIteratorRecord());
            }

            registerFile[code->m_dstIndex] = result;
            ADD_PROGRAM_COUNTER(BindingRestElement);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ExecutionResume)
            :
        {
            Value v = executionResumeOperation(state, programCounter, byteCodeBlock);
            if (!v.isEmpty()) {
                return v;
            }
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(ExecutionPause)
            :
        {
            ExecutionPause* code = (ExecutionPause*)programCounter;
            executionPauseOperation(*state, registerFile, programCounter, codeBuffer);
            return Value();
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

        DEFINE_OPCODE(TaggedTemplateOperation)
            :
        {
            taggedTemplateOperation(*state, programCounter, registerFile, codeBuffer, byteCodeBlock);
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

        DEFINE_OPCODE(BreakpointDisabled)
            :
        {
#ifdef ESCARGOT_DEBUGGER
            if (state->context()->debuggerEnabled()) {
                state->context()->debugger()->processDisabledBreakpoint(byteCodeBlock, (uint32_t)(programCounter - (size_t)codeBuffer), state);
            }
#endif /* ESCARGOT_DEBUGGER */

            ADD_PROGRAM_COUNTER(BreakpointDisabled);
            NEXT_INSTRUCTION();
        }

        DEFINE_OPCODE(BreakpointEnabled)
            :
        {
#ifdef ESCARGOT_DEBUGGER
            if (state->context()->debuggerEnabled()) {
                state->context()->debugger()->stopAtBreakpoint(byteCodeBlock, (uint32_t)(programCounter - (size_t)codeBuffer), state);
            }
#endif /* ESCARGOT_DEBUGGER */

            ADD_PROGRAM_COUNTER(BreakpointEnabled);
            NEXT_INSTRUCTION();
        }

        DEFINE_DEFAULT
    }

    ASSERT_NOT_REACHED();

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
FillOpcodeTableLbl:
#if defined(ENABLE_CODE_CACHE)
#define REGISTER_TABLE(opcode, pushCount, popCount)                     \
    g_opcodeTable.m_addressTable[opcode##Opcode] = &&opcode##OpcodeLbl; \
    g_opcodeTable.m_opcodeMap.insert(std::make_pair(&&opcode##OpcodeLbl, (size_t)opcode##Opcode));
#else
#define REGISTER_TABLE(opcode, pushCount, popCount) \
    g_opcodeTable.m_addressTable[opcode##Opcode] = &&opcode##OpcodeLbl;
#endif
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
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);

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
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);

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
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, code->m_name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);
    }
    GlobalObject* o = state.context()->globalObject();
    o->setThrowsExceptionWhenStrictMode(state, code->m_name, value, o);
}

NEVER_INLINE Value ByteCodeInterpreter::plusSlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);
    Value lval(Value::ForceUninitialized);
    Value rval(Value::ForceUninitialized);

    // https://www.ecma-international.org/ecma-262/#sec-addition-operator-plus
    // Let lref be the result of evaluating AdditiveExpression.
    // Let lval be ? GetValue(lref).
    // Let rref be the result of evaluating MultiplicativeExpression.
    // Let rval be ? GetValue(rref).
    // Let lprim be ? ToPrimitive(lval).
    // Let rprim be ? ToPrimitive(rval).
    lval = left.toPrimitive(state);
    rval = right.toPrimitive(state);

    // If Type(lprim) is String or Type(rprim) is String, then
    if (lval.isString() || rval.isString()) {
        // Let lstr be ? ToString(lprim).
        // Let rstr be ? ToString(rprim).
        // Return the string-concatenation of lstr and rstr.
        ret = RopeString::createRopeString(lval.toString(state), rval.toString(state), &state);
    } else {
        // Let lnum be ? ToNumeric(lprim).
        auto lnum = lval.toNumeric(state);
        // Let rnum be ? ToNumeric(rprim).
        auto rnum = rval.toNumeric(state);
        // If Type(lnum) is different from Type(rnum), throw a TypeError exception.
        if (UNLIKELY(lnum.second != rnum.second)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
        }
        // Let T be Type(lnum).
        // Return T::add(lnum, rnum).
        if (UNLIKELY(lnum.second)) {
            ret = Value(lnum.first.asBigInt()->addition(state, rnum.first.asBigInt()));
        } else {
            ret = Value(lnum.first.asNumber() + rnum.first.asNumber());
        }
    }

    return ret;
}

NEVER_INLINE Value ByteCodeInterpreter::minusSlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    // https://www.ecma-international.org/ecma-262/#sec-subtraction-operator-minus
    // Let lref be the result of evaluating AdditiveExpression.
    // Let lval be ? GetValue(lref).
    // Let rref be the result of evaluating MultiplicativeExpression.
    // Let rval be ? GetValue(rref).
    // Let lnum be ? ToNumeric(lval).
    // Let rnum be ? ToNumeric(rval).
    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    // If Type(lnum) is different from Type(rnum), throw a TypeError exception.
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    // Let T be Type(lnum).
    // Return T::subtract(lnum, rnum).
    if (UNLIKELY(lnum.second)) {
        return Value(lnum.first.asBigInt()->subtraction(state, rnum.first.asBigInt()));
    } else {
        return Value(lnum.first.asNumber() - rnum.first.asNumber());
    }
}

NEVER_INLINE Value ByteCodeInterpreter::multiplySlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    if (UNLIKELY(lnum.second)) {
        return Value(lnum.first.asBigInt()->multiply(state, rnum.first.asBigInt()));
    } else {
        return Value(lnum.first.asNumber() * rnum.first.asNumber());
    }
}

NEVER_INLINE Value ByteCodeInterpreter::divisionSlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    if (UNLIKELY(lnum.second)) {
        if (UNLIKELY(rnum.first.asBigInt()->isZero())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::DivisionByZero);
        }
        return Value(lnum.first.asBigInt()->division(state, rnum.first.asBigInt()));
    } else {
        return Value(lnum.first.asNumber() / rnum.first.asNumber());
    }
}

NEVER_INLINE Value ByteCodeInterpreter::unaryMinusSlowCase(ExecutionState& state, const Value& src)
{
    auto r = src.toNumeric(state);
    if (r.second) {
        return r.first.asBigInt()->negativeValue(state);
    } else {
        return Value(-r.first.asNumber());
    }
}

NEVER_INLINE Value ByteCodeInterpreter::modOperation(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);

    int32_t intLeft;
    int32_t intRight;
    if (left.isInt32() && ((intLeft = left.asInt32()) > 0) && right.isInt32() && (intRight = right.asInt32())) {
        ret = Value(intLeft % intRight);
    } else {
        auto lnum = left.toNumeric(state);
        auto rnum = right.toNumeric(state);
        if (UNLIKELY(lnum.second != rnum.second)) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
        }
        if (UNLIKELY(lnum.second)) {
            if (UNLIKELY(rnum.first.asBigInt()->isZero())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::DivisionByZero);
            }
            return Value(lnum.first.asBigInt()->remainder(state, rnum.first.asBigInt()));
        }

        double lvalue = lnum.first.asNumber();
        double rvalue = rnum.first.asNumber();
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

NEVER_INLINE Value ByteCodeInterpreter::exponentialOperation(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);

    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    if (UNLIKELY(lnum.second)) {
        if (UNLIKELY(rnum.first.asBigInt()->isNegative())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, ErrorObject::Messages::ExponentByNegative);
        }
        return Value(lnum.first.asBigInt()->pow(state, rnum.first.asBigInt()));
    }

    double base = lnum.first.asNumber();
    double exp = rnum.first.asNumber();

    // The result of base ** exponent when base is 1 or -1 and exponent is +Infinity or -Infinity differs from IEEE 754-2008. The first edition of ECMAScript specified a result of NaN for this operation, whereas later versions of IEEE 754-2008 specified 1. The historical ECMAScript behaviour is preserved for compatibility reasons.
    if ((base == -1 || base == 1) && (exp == std::numeric_limits<double>::infinity() || exp == -std::numeric_limits<double>::infinity())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }

    return Value(pow(base, exp));
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

NEVER_INLINE Value ByteCodeInterpreter::bitwiseOperationSlowCase(ExecutionState& state, const Value& left, const Value& right, ByteCodeInterpreter::BitwiseOperationKind kind)
{
    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    if (UNLIKELY(lnum.second)) {
        switch (kind) {
        case BitwiseOperationKind::And:
            return lnum.first.asBigInt()->bitwiseAnd(state, rnum.first.asBigInt());
        case BitwiseOperationKind::Or:
            return lnum.first.asBigInt()->bitwiseOr(state, rnum.first.asBigInt());
        case BitwiseOperationKind::Xor:
            return lnum.first.asBigInt()->bitwiseXor(state, rnum.first.asBigInt());
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (kind) {
        case BitwiseOperationKind::And:
            return Value(lnum.first.toInt32(state) & rnum.first.toInt32(state));
        case BitwiseOperationKind::Or:
            return Value(lnum.first.toInt32(state) | rnum.first.toInt32(state));
        case BitwiseOperationKind::Xor:
            return Value(lnum.first.toInt32(state) ^ rnum.first.toInt32(state));
        default:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT_NOT_REACHED();
    return Value();
}

NEVER_INLINE Value ByteCodeInterpreter::bitwiseNotOperationSlowCase(ExecutionState& state, const Value& a)
{
    auto r = a.toNumeric(state);
    if (r.second) {
        return r.first.asBigInt()->bitwiseNot(state);
    } else {
        return Value(~r.first.toInt32(state));
    }
}

NEVER_INLINE Value ByteCodeInterpreter::shiftOperationSlowCase(ExecutionState& state, const Value& left, const Value& right, ByteCodeInterpreter::ShiftOperationKind kind)
{
    auto lnum = left.toNumeric(state);
    auto rnum = right.toNumeric(state);
    if (UNLIKELY(lnum.second != rnum.second)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::CanNotMixBigIntWithOtherTypes);
    }
    if (UNLIKELY(lnum.second)) {
        switch (kind) {
        case ShiftOperationKind::Left:
            return lnum.first.asBigInt()->leftShift(state, rnum.first.asBigInt());
        case ShiftOperationKind::SignedRight:
            return lnum.first.asBigInt()->rightShift(state, rnum.first.asBigInt());
        case ShiftOperationKind::UnsignedRight:
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "BigInts have no unsigned right shift, use >> instead");
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        switch (kind) {
        case ShiftOperationKind::Left: {
            int32_t lnum32 = lnum.first.toInt32(state);
            int32_t rnum32 = rnum.first.toInt32(state);
            lnum32 <<= ((unsigned int)rnum32) & 0x1F;
            return Value(lnum32);
        }
        case ShiftOperationKind::SignedRight: {
            int32_t lnum32 = lnum.first.toInt32(state);
            int32_t rnum32 = rnum.first.toInt32(state);
            lnum32 >>= ((unsigned int)rnum32) & 0x1F;
            return Value(lnum32);
        }
        case ShiftOperationKind::UnsignedRight: {
            uint32_t lnum32 = lnum.first.toUint32(state);
            uint32_t rnum32 = rnum.first.toUint32(state);
            lnum32 = (lnum32) >> ((rnum32)&0x1F);
            return Value(lnum32);
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }

    ASSERT_NOT_REACHED();
    return Value();
}

NEVER_INLINE void ByteCodeInterpreter::deleteOperation(ExecutionState& state, LexicalEnvironment* env, UnaryDelete* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
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
    } else if (code->m_hasSuperExpression) {
        if (byteCodeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
            EnvironmentRecord* envRec = state.getThisEnvironment();
            ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
            envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state); // check thisbinding
        }
        const Value& p = registerFile[code->m_srcIndex1];
        auto name = ObjectPropertyName(state, p);

        const Value& o = state.makeSuperPropertyReference();
        Object* obj = o.toObject(state);

        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.toObjectStructurePropertyName(state).toExceptionString(), false, String::emptyString, "ReferenceError: Unsupported reference to 'super'");
    } else {
        const Value& o = registerFile[code->m_srcIndex0];
        const Value& p = registerFile[code->m_srcIndex1];

        auto name = ObjectPropertyName(state, p);
        Object* obj = o.toObject(state);

        bool result = obj->deleteOwnProperty(state, name);
        if (!result && state.inStrictMode())
            Object::throwCannotDeleteError(state, name.toObjectStructurePropertyName(state));
        registerFile[code->m_dstIndex] = Value(result);
    }
}

ALWAYS_INLINE bool ByteCodeInterpreter::abstractLeftIsLessThanRight(ExecutionState& state, const Value& left, const Value& right, bool switched)
{
    // consume very fast case
    if (LIKELY(left.isInt32() && right.isInt32())) {
        return left.asInt32() < right.asInt32();
    }

    if (LIKELY(left.isNumber() && right.isNumber())) {
        return left.asNumber() < right.asNumber();
    }

    return abstractLeftIsLessThanRightSlowCase(state, left, right, switched);
}

ALWAYS_INLINE bool ByteCodeInterpreter::abstractLeftIsLessThanEqualRight(ExecutionState& state, const Value& left, const Value& right, bool switched)
{
    // consume very fast case
    if (LIKELY(left.isInt32() && right.isInt32())) {
        return left.asInt32() <= right.asInt32();
    }

    if (LIKELY(left.isNumber() && right.isNumber())) {
        return left.asNumber() <= right.asNumber();
    }

    return abstractLeftIsLessThanEqualRightSlowCase(state, left, right, switched);
}

template <typename CompBigIntBigInt, typename CompBigIntBigIntData, typename CompBigIntDataBigInt, typename CompDoubleDouble>
ALWAYS_INLINE bool abstractLeftIsLessThanRightNumeric(ExecutionState& state, const Value& lval, const Value& rval,
                                                      CompBigIntBigInt compBigIntBigInt, CompBigIntBigIntData compBigIntBigIntData,
                                                      CompBigIntDataBigInt compBigIntDataBigInt, CompDoubleDouble compDoubleDouble)
{
    bool lvalIsBigInt = lval.isBigInt();
    bool rvalIsBigInt = rval.isBigInt();
    bool lvalIsString = lval.isString();
    bool rvalIsString = rval.isString();

    // If Type(px) is BigInt and Type(py) is String, then
    if (UNLIKELY(lvalIsBigInt && rvalIsString)) {
        // Let ny be StringToBigInt(py).
        BigIntData ny(rval.asString());
        // If ny is NaN, return undefined.
        if (ny.isNaN()) {
            return false;
        }
        // Return BigInt::...(px, ny).
        return compBigIntBigIntData(lval.asBigInt(), ny);
    }

    // If Type(px) is String and Type(py) is BigInt, then
    if (UNLIKELY(lvalIsString && rvalIsBigInt)) {
        // Let nx be StringToBigInt(px).
        BigIntData nx(lval.asString());
        // If nx is NaN, return undefined.
        if (nx.isNaN()) {
            return false;
        }
        // Return BigInt::...(nx, py).
        return compBigIntDataBigInt(nx, rval.asBigInt());
    }

    // Let nx be ? ToNumeric(px). NOTE: Because px and py are primitive values evaluation order is not important.
    auto nx = lval.toNumeric(state);
    // Let ny be ? ToNumeric(py).
    auto ny = rval.toNumeric(state);

    // If Type(nx) is the same as Type(ny), return Type(nx)::lessThan(nx, ny).
    // Assert: Type(nx) is BigInt and Type(ny) is Number, or Type(nx) is Number and Type(ny) is BigInt.
    // If nx or ny is NaN, return undefined.
    // If nx is -∞ or ny is +∞, return true.
    // If nx is +∞ or ny is -∞, return false.
    // If the mathematical value of nx is ... than the mathematical value of ny, return true, otherwise return false.

    if (UNLIKELY(nx.second)) {
        if (UNLIKELY(ny.second)) {
            return compBigIntBigInt(nx.first.asBigInt(), ny.first.asBigInt());
        } else {
            return compBigIntBigIntData(nx.first.asBigInt(), BigIntData(ny.first.asNumber()));
        }
    } else {
        if (UNLIKELY(ny.second)) {
            return compBigIntDataBigInt(BigIntData(nx.first.asNumber()), ny.first.asBigInt());
        } else {
            return compDoubleDouble(nx.first.asNumber(), ny.first.asNumber());
        }
    }
}

NEVER_INLINE bool ByteCodeInterpreter::abstractLeftIsLessThanRightSlowCase(ExecutionState& state, const Value& left, const Value& right, bool switched)
{
    Value lval, rval;
    if (switched) {
        rval = right.toPrimitive(state, Value::PreferNumber);
        lval = left.toPrimitive(state, Value::PreferNumber);
    } else {
        lval = left.toPrimitive(state, Value::PreferNumber);
        rval = right.toPrimitive(state, Value::PreferNumber);
    }

    if (lval.isInt32() && rval.isInt32()) {
        return lval.asInt32() < rval.asInt32();
    } else if (lval.isString() && rval.isString()) {
        return *lval.asString() < *rval.asString();
    } else {
        return abstractLeftIsLessThanRightNumeric(state, lval, rval,
                                                  [](BigInt* a, BigInt* b) -> bool {
                                                      return a->lessThan(b);
                                                  },
                                                  [](BigInt* a, const BigIntData& b) -> bool {
                                                      return a->lessThan(b);
                                                  },
                                                  [](const BigIntData& a, BigInt* b) -> bool {
                                                      return a.lessThan(b);
                                                  },
                                                  [](const double& a, const double& b) -> bool {
                                                      return a < b;
                                                  });
    }
}

NEVER_INLINE bool ByteCodeInterpreter::abstractLeftIsLessThanEqualRightSlowCase(ExecutionState& state, const Value& left, const Value& right, bool switched)
{
    Value lval, rval;
    if (switched) {
        rval = right.toPrimitive(state, Value::PreferNumber);
        lval = left.toPrimitive(state, Value::PreferNumber);
    } else {
        lval = left.toPrimitive(state, Value::PreferNumber);
        rval = right.toPrimitive(state, Value::PreferNumber);
    }

    if (lval.isInt32() && rval.isInt32()) {
        return lval.asInt32() <= rval.asInt32();
    } else if (lval.isString() && rval.isString()) {
        return *lval.asString() <= *rval.asString();
    } else {
        return abstractLeftIsLessThanRightNumeric(state, lval, rval,
                                                  [](BigInt* a, BigInt* b) -> bool {
                                                      return a->lessThanEqual(b);
                                                  },
                                                  [](BigInt* a, const BigIntData& b) -> bool {
                                                      return a->lessThanEqual(b);
                                                  },
                                                  [](const BigIntData& a, BigInt* b) -> bool {
                                                      return a.lessThanEqual(b);
                                                  },
                                                  [](const double& a, const double& b) -> bool {
                                                      return a <= b;
                                                  });
    }
}

ALWAYS_INLINE void ByteCodeInterpreter::getObjectPrecomputedCaseOperation(ExecutionState& state, GetObjectPreComputedCase* code, Value* registerFile, ByteCodeBlock* block)
{
    const Value& receiver = registerFile[code->m_objectRegisterIndex];
    Object* obj;
    if (LIKELY(receiver.isObject())) {
        obj = receiver.asObject();
    } else {
        obj = fastToObject(state, receiver);
    }

    if (LIKELY(code->m_inlineCacheMode == GetObjectPreComputedCase::Simple)) {
        auto cacheData = code->m_simpleInlineCache->m_cachedStructures;
        ObjectStructure* const objStructure = obj->structure();
        // NOTE
        // if GetObjectInlineCacheSimpleCase::simpleCaseLimit is small,
        // `skipping cacheData[currentCacheIndex] != nullptr` is faster
        for (unsigned currentCacheIndex = 0; /* cacheData[currentCacheIndex] &&*/ currentCacheIndex < GetObjectInlineCacheSimpleCase::inlineBufferSize; currentCacheIndex++) {
            if (cacheData[currentCacheIndex] == objStructure) {
                registerFile[code->m_storeRegisterIndex] = obj->m_values[code->m_simpleInlineCache->m_cachedIndexes[currentCacheIndex]];
                return;
            }
        }
    } else if (code->m_inlineCacheMode == GetObjectPreComputedCase::Complex) {
        if (getObjectPrecomputedCaseOperationSlowCase(state, obj, receiver, code, registerFile, block)) {
            return;
        }
    }

    getObjectPrecomputedCaseOperationCacheMiss(state, obj, receiver, code, registerFile, block);
}

NEVER_INLINE bool ByteCodeInterpreter::getObjectPrecomputedCaseOperationSlowCase(ExecutionState& state, Object* obj, const Value& receiver, GetObjectPreComputedCase* code,
                                                                                 Value* registerFile,
                                                                                 ByteCodeBlock* block)
{
    GetObjectInlineCacheComplexCase* const inlineCache = code->m_complexInlineCache;
    const size_t cacheFillCount = inlineCache->m_cache.size();
    GetObjectInlineCacheData* const cacheData = inlineCache->m_cache.data();
    Object* objChain[GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount];
    ObjectStructure* objStructures[GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount];
    const auto& maxIndex = code->m_inlineCacheProtoTraverseMaxIndex;
    size_t fillCount;
    for (fillCount = 0; fillCount <= maxIndex && obj; fillCount++) {
        objChain[fillCount] = obj;
        objStructures[fillCount] = obj->structure();
        obj = obj->Object::getPrototypeObject(state);
    }

    for (unsigned currentCacheIndex = 0; currentCacheIndex < cacheFillCount; currentCacheIndex++) {
        const GetObjectInlineCacheData& data = cacheData[currentCacheIndex];
        const size_t& cSiz = data.m_cachedhiddenClassChainLength;
        if (LIKELY(cSiz <= fillCount)) {
            bool ok = true;
            for (size_t i = 0; i < cSiz; i++) {
                if (objStructures[i] != data.m_cachedhiddenClassChain[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                const auto& cachedIndex = data.m_cachedIndex;
                if (LIKELY(cachedIndex != GetObjectInlineCacheData::inlineCacheCachedIndexMax)) {
                    if (LIKELY(data.m_isPlainDataProperty)) {
                        registerFile[code->m_storeRegisterIndex] = objChain[cSiz - 1]->m_values[cachedIndex];
                    } else {
                        registerFile[code->m_storeRegisterIndex] = objChain[cSiz - 1]->getOwnNonPlainDataPropertyUtilForObject(state, cachedIndex, receiver);
                    }
                } else {
                    registerFile[code->m_storeRegisterIndex] = Value();
                }
                return true;
            }
        }
    }
    return false;
}

NEVER_INLINE void ByteCodeInterpreter::getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& receiver, GetObjectPreComputedCase* code, Value* registerFile, ByteCodeBlock* block)
{
    if (code->m_isLength && obj->isArrayObject()) {
        registerFile[code->m_storeRegisterIndex] = Value(obj->asArrayObject()->arrayLength(state));
        return;
    }

#if defined(ESCARGOT_SMALL_CONFIG)
    registerFile[code->m_storeRegisterIndex] = obj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
    return;
#endif

    const size_t maxCacheMissCount = 32;
    const size_t minCacheFillCount = 4;
    const size_t maxCacheCount = 24;

    // cache miss.
    if (code->m_cacheMissCount > maxCacheMissCount) {
        registerFile[code->m_storeRegisterIndex] = obj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
        return;
    }

    code->m_cacheMissCount++;
    if (code->m_cacheMissCount <= minCacheFillCount) {
        registerFile[code->m_storeRegisterIndex] = obj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
        return;
    }

    if (UNLIKELY(!obj->isInlineCacheable())) {
        code->m_cacheMissCount = maxCacheMissCount + 1;
        registerFile[code->m_storeRegisterIndex] = obj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
        return;
    }

    if (UNLIKELY(code->m_cacheMissCount == maxCacheMissCount)) {
        registerFile[code->m_storeRegisterIndex] = obj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
        return;
    }

    Object* orgObj = obj;

    auto& currentCodeSizeTotal = state.context()->vmInstance()->compiledByteCodeSize();
    VectorWithInlineStorage<GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount, ObjectStructure*, std::allocator<ObjectStructure*>> cachedhiddenClassChain;
    size_t cachedIndex = 0;
    bool isPlainDataProperty = 0;

    while (true) {
        auto s = obj->structure();
        cachedhiddenClassChain.push_back(s);
        auto result = s->findProperty(code->m_propertyName);

        if (result.first != SIZE_MAX) {
            if (UNLIKELY(result.first > GetObjectInlineCacheData::inlineCacheCachedIndexMax)) {
                goto GiveUp;
            }
            cachedIndex = result.first;
            isPlainDataProperty = result.second->m_descriptor.isPlainDataProperty();
            break;
        }

        obj = obj->Object::getPrototypeObject(state);

        if (!obj) {
            cachedIndex = GetObjectInlineCacheData::inlineCacheCachedIndexMax;
            break;
        }

        if (UNLIKELY(!obj->isInlineCacheable())) {
            goto GiveUp;
        }
    }

    if (UNLIKELY(cachedhiddenClassChain.size() > GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount)) {
        goto GiveUp;
    }

    // this is possible. but super rare
    // so this case is not cached for performance
    if (UNLIKELY(cachedhiddenClassChain.size() == 1 && cachedIndex == GetObjectInlineCacheData::inlineCacheCachedIndexMax)) {
        goto GiveUp;
    }

    if (isPlainDataProperty && cachedhiddenClassChain.size() == 1 && cachedIndex <= std::numeric_limits<uint8_t>::max()
        && code->m_inlineCacheMode <= GetObjectPreComputedCase::Simple) {
        if (code->m_inlineCacheMode != GetObjectPreComputedCase::Simple) {
            code->m_simpleInlineCache = new GetObjectInlineCacheSimpleCase();
            block->m_inlineCacheDataSize += sizeof(GetObjectInlineCacheSimpleCase);
            currentCodeSizeTotal += sizeof(GetObjectInlineCacheSimpleCase);
            code->m_inlineCacheMode = GetObjectPreComputedCase::Simple;
            block->m_otherLiteralData.push_back(code->m_simpleInlineCache);
        }

        auto inlineCache = code->m_simpleInlineCache;

        size_t targetIndex = 0;
        for (; targetIndex < GetObjectInlineCacheSimpleCase::inlineBufferSize; targetIndex++) {
            if (inlineCache->m_cachedStructures[targetIndex] == nullptr) {
                break;
            }
        }
        if (targetIndex == GetObjectInlineCacheSimpleCase::inlineBufferSize) {
            for (size_t i = GetObjectInlineCacheSimpleCase::inlineBufferSize - 1; i > 0; i--) {
                inlineCache->m_cachedStructures[i] = inlineCache->m_cachedStructures[i - 1];
                inlineCache->m_cachedIndexes[i] = inlineCache->m_cachedIndexes[i - 1];
            }
            targetIndex = 0;
        }
        inlineCache->m_cachedStructures[targetIndex] = cachedhiddenClassChain[0];
        inlineCache->m_cachedIndexes[targetIndex] = cachedIndex;

        registerFile[code->m_storeRegisterIndex] = obj->m_values[cachedIndex];
    } else {
        if (code->m_inlineCacheMode == GetObjectPreComputedCase::Simple) {
            // convert simple case to complex case
            GetObjectInlineCacheSimpleCase* old = code->m_simpleInlineCache;
            auto inlineCache = code->m_complexInlineCache = new GetObjectInlineCacheComplexCase();
            block->m_otherLiteralData.push_back(code->m_complexInlineCache);
            code->m_inlineCacheMode = GetObjectPreComputedCase::Complex;
            block->m_inlineCacheDataSize += sizeof(GetObjectInlineCacheComplexCase) - sizeof(GetObjectInlineCacheSimpleCase);
            currentCodeSizeTotal += sizeof(GetObjectInlineCacheComplexCase) - sizeof(GetObjectInlineCacheSimpleCase);
            for (size_t i = 0; i < GetObjectInlineCacheSimpleCase::inlineBufferSize && old->m_cachedStructures[i]; i++) {
                inlineCache->m_cache.pushBack(GetObjectInlineCacheData());
                block->m_inlineCacheDataSize += sizeof(GetObjectInlineCacheData);
                currentCodeSizeTotal += sizeof(GetObjectInlineCacheData);

                auto& item = inlineCache->m_cache.back();
                item.m_cachedhiddenClassChain = (ObjectStructure**)GC_MALLOC(sizeof(ObjectStructure*));
                item.m_cachedhiddenClassChain[0] = old->m_cachedStructures[i];
                item.m_cachedhiddenClassChainLength = 1;
                item.m_cachedIndex = old->m_cachedIndexes[i];
                item.m_isPlainDataProperty = true;
            }
        } else if (code->m_inlineCacheMode == GetObjectPreComputedCase::None) {
            code->m_complexInlineCache = new GetObjectInlineCacheComplexCase();
            block->m_otherLiteralData.push_back(code->m_complexInlineCache);
            code->m_inlineCacheMode = GetObjectPreComputedCase::Complex;
            block->m_inlineCacheDataSize += sizeof(GetObjectInlineCacheComplexCase);
            currentCodeSizeTotal += sizeof(GetObjectInlineCacheComplexCase);
        }

        auto inlineCache = code->m_complexInlineCache;
        if (inlineCache->m_cache.size() > maxCacheCount) {
            for (size_t i = inlineCache->m_cache.size() - 1; i > 0; i--) {
                inlineCache->m_cache[i] = inlineCache->m_cache[i - 1];
            }
        } else {
            inlineCache->m_cache.insert(0, GetObjectInlineCacheData());
            block->m_inlineCacheDataSize += sizeof(GetObjectInlineCacheData);
            currentCodeSizeTotal += sizeof(GetObjectInlineCacheData);
        }

        auto& newItem = inlineCache->m_cache[0];
        code->m_inlineCacheProtoTraverseMaxIndex = std::max(cachedhiddenClassChain.size() - 1, (size_t)code->m_inlineCacheProtoTraverseMaxIndex);

        newItem.m_cachedhiddenClassChainLength = cachedhiddenClassChain.size();
        block->m_inlineCacheDataSize += sizeof(size_t) * cachedhiddenClassChain.size();
        currentCodeSizeTotal += sizeof(size_t) * cachedhiddenClassChain.size();
        newItem.m_cachedhiddenClassChain = (ObjectStructure**)GC_MALLOC(sizeof(ObjectStructure*) * cachedhiddenClassChain.size());
        memcpy(newItem.m_cachedhiddenClassChain, cachedhiddenClassChain.data(), sizeof(ObjectStructure*) * cachedhiddenClassChain.size());
        newItem.m_cachedIndex = cachedIndex;
        newItem.m_isPlainDataProperty = isPlainDataProperty;

        if (newItem.m_cachedIndex != GetObjectInlineCacheData::inlineCacheCachedIndexMax) {
            if (newItem.m_isPlainDataProperty) {
                registerFile[code->m_storeRegisterIndex] = obj->m_values[newItem.m_cachedIndex];
            } else {
                registerFile[code->m_storeRegisterIndex] = obj->getOwnNonPlainDataPropertyUtilForObject(state, newItem.m_cachedIndex, receiver);
            }
        } else {
            registerFile[code->m_storeRegisterIndex] = Value();
        }
    }

    return;

GiveUp:
    code->m_inlineCacheMode = GetObjectPreComputedCase::None;
    code->m_simpleInlineCache = nullptr;
    code->m_cacheMissCount = maxCacheMissCount + 1;
    registerFile[code->m_storeRegisterIndex] = orgObj->get(state, ObjectPropertyName(state, code->m_propertyName)).value(state, receiver);
}

ALWAYS_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperation(ExecutionState& state, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block)
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
    auto inlineCache = code->m_inlineCache;

    if (LIKELY(inlineCache != nullptr)) {
        // simple case
        if (LIKELY(code->m_inlineCacheProtoTraverseMaxIndex == 0)) {
            ObjectStructure* testItem = obj->structure();
            SetObjectInlineCache* const inlineCache = code->m_inlineCache;
            const size_t cacheFillCount = inlineCache->m_cache.size();
            SetObjectInlineCacheData* const cacheData = inlineCache->m_cache.data();
            for (unsigned currentCacheIndex = 0; currentCacheIndex < cacheFillCount; currentCacheIndex++) {
                const auto& item = cacheData[currentCacheIndex];
                if (testItem == item.m_cachedHiddenClass) {
                    // cache hit!
                    obj->m_values[item.m_cachedIndex] = value;
                    return;
                }
            }
        } else {
            if (setObjectPreComputedCaseOperationSlowCase(state, originalObject, willBeObject, value, code, block)) {
                return;
            }
        }
    }

    setObjectPreComputedCaseOperationCacheMiss(state, originalObject, willBeObject, value, code, block);
}

NEVER_INLINE bool ByteCodeInterpreter::setObjectPreComputedCaseOperationSlowCase(ExecutionState& state, Object* originalObject, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block)
{
    Object* obj = originalObject;
    Object* objChain[GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount];
    ObjectStructure* objStructures[GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount];
    const auto& maxIndex = code->m_inlineCacheProtoTraverseMaxIndex;
    size_t fillCount;
    for (fillCount = 0; fillCount <= maxIndex && obj; fillCount++) {
        objChain[fillCount] = obj;
        objStructures[fillCount] = obj->structure();
        obj = obj->Object::getPrototypeObject(state);
    }

    SetObjectInlineCache* const inlineCache = code->m_inlineCache;
    const size_t cacheFillCount = inlineCache->m_cache.size();
    SetObjectInlineCacheData* const cacheData = inlineCache->m_cache.data();
    for (unsigned currentCacheIndex = 0; currentCacheIndex < cacheFillCount; currentCacheIndex++) {
        const auto& item = cacheData[currentCacheIndex];
        const auto& cSiz = item.m_cachedhiddenClassChainLength;
        bool ok = true;
        for (size_t i = 0; i < cSiz; i++) {
            if (objStructures[i] != item.m_cachedHiddenClassChainData[i]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            if (item.m_cachedIndex != SetObjectInlineCacheData::inlineCacheCachedIndexMax) {
                originalObject->m_values[item.m_cachedIndex] = value;
            } else {
                ASSERT(originalObject->structure()->inTransitionMode());
                // next object structure save in `item.m_cachedHiddenClassChainData[cSiz]`
                originalObject->m_structure = item.m_cachedHiddenClassChainData[cSiz];
                originalObject->m_values.push_back(value, originalObject->m_structure->propertyCount());
            }
            return true;
        }
    }
    return false;
}

NEVER_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* originalObject, const Value& willBeObject, const Value& value, SetObjectPreComputedCase* code, ByteCodeBlock* block)
{
    if (code->m_isLength && originalObject->isArrayObject()) {
        if (LIKELY(originalObject->asArrayObject()->isFastModeArray())) {
            if (!originalObject->asArrayObject()->setArrayLength(state, value) && state.inStrictMode()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, code->m_propertyName.toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
            }
        } else {
            originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
        }
        return;
    }

#if defined(ESCARGOT_SMALL_CONFIG)
    originalObject->markThisObjectDontNeedStructureTransitionTable();
    originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
    return;
#endif

    const size_t maxCacheMissCount = 32;
    const size_t minCacheFillCount = 3;
    const size_t maxCacheCount = 24;

    // cache miss
    if (code->m_missCount > maxCacheMissCount) {
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
        return;
    }

    if (code->m_missCount < minCacheFillCount) {
        code->m_missCount++;
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
        return;
    }

    if (UNLIKELY(!originalObject->isInlineCacheable())) {
        code->m_missCount = maxCacheMissCount + 1;
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
        return;
    }

    auto& currentCodeSizeTotal = state.context()->vmInstance()->compiledByteCodeSize();

    if (code->m_inlineCache == nullptr) {
        code->m_inlineCache = new SetObjectInlineCache();
        block->m_inlineCacheDataSize += sizeof(SetObjectInlineCache);
        currentCodeSizeTotal += sizeof(SetObjectInlineCache);
        block->m_otherLiteralData.push_back(code->m_inlineCache);
    }

    auto inlineCache = code->m_inlineCache;

    if (inlineCache->m_cache.size() > maxCacheCount) {
        for (size_t i = inlineCache->m_cache.size() - 1; i > 0; i--) {
            inlineCache->m_cache[i] = inlineCache->m_cache[i - 1];
        }
    } else {
        inlineCache->m_cache.insert(0, SetObjectInlineCacheData());
        block->m_inlineCacheDataSize += sizeof(SetObjectInlineCacheData);
        currentCodeSizeTotal += sizeof(SetObjectInlineCacheData);
    }

    auto& newItem = inlineCache->m_cache[0];
    VectorWithInlineStorage<GetObjectPreComputedCase::inlineCacheProtoTraverseMaxCount, ObjectStructure*, std::allocator<ObjectStructure*>> cachedhiddenClassChain;
    Object* obj = originalObject;

    auto findResult = obj->structure()->findProperty(code->m_propertyName);
    if (findResult.first != SIZE_MAX) {
        // own property
        obj->setOwnPropertyThrowsExceptionWhenStrictMode(state, findResult.first, value, willBeObject);
        // Don't update the inline cache if the property is removed by a setter function.
        /* example code
        var o = { set foo (a) { var a = delete o.foo } };
        o.foo = 0;
        */
        if (UNLIKELY(findResult.first >= obj->structure()->propertyCount())) {
            goto GiveUp;
        }

        const auto& propertyData = obj->structure()->readProperty(findResult.first);
        const auto& desc = propertyData.m_descriptor;
        if (propertyData.m_propertyName == code->m_propertyName && desc.isPlainDataProperty() && desc.isWritable()) {
            if (code->m_inlineCacheProtoTraverseMaxIndex == 0) {
                newItem.m_cachedIndex = findResult.first;
                newItem.m_cachedhiddenClassChainLength = 1;
                newItem.m_cachedHiddenClass = obj->structure();
            } else {
                newItem.m_cachedIndex = findResult.first;
                newItem.m_cachedhiddenClassChainLength = 1;
                newItem.m_cachedHiddenClassChainData = (ObjectStructure**)GC_MALLOC(sizeof(ObjectStructure*));
                newItem.m_cachedHiddenClassChainData[0] = obj->structure();
            }
        } else {
            goto GiveUp;
        }
    } else {
        Object* orgObject = obj;
        if (UNLIKELY(!obj->structure()->inTransitionMode())) {
            orgObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
            goto GiveUp;
        }

        VectorWithInlineStorage<24, ObjectStructure*, std::allocator<ObjectStructure*>> cachedhiddenClassChain;
        cachedhiddenClassChain.push_back(obj->structure());
        Value proto = obj->getPrototype(state);
        while (proto.isObject()) {
            obj = proto.asObject();

            if (!UNLIKELY(obj->isInlineCacheable())) {
                code->m_missCount++;
                originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
                goto GiveUp;
            }

            cachedhiddenClassChain.push_back(obj->structure());
            proto = obj->getPrototype(state);
        }

        newItem.m_cachedIndex = SetObjectInlineCacheData::inlineCacheCachedIndexMax;
        newItem.m_cachedhiddenClassChainLength = cachedhiddenClassChain.size();
        // +1 space for next object structure
        newItem.m_cachedHiddenClassChainData = (ObjectStructure**)GC_MALLOC(sizeof(ObjectStructure*) * (newItem.m_cachedhiddenClassChainLength + 1));
        memcpy(newItem.m_cachedHiddenClassChainData, cachedhiddenClassChain.data(), sizeof(ObjectStructure*) * newItem.m_cachedhiddenClassChainLength);

        block->m_inlineCacheDataSize += sizeof(size_t) * newItem.m_cachedhiddenClassChainLength;
        currentCodeSizeTotal += sizeof(size_t) * newItem.m_cachedhiddenClassChainLength;
        bool s = orgObject->set(state, ObjectPropertyName(state, code->m_propertyName), value, willBeObject);
        if (UNLIKELY(!s)) {
            inlineCache->m_cache.clear();
            code->m_inlineCache = nullptr;
            code->m_missCount = maxCacheMissCount + 1;
            if (state.inStrictMode()) {
                orgObject->throwCannotWriteError(state, code->m_propertyName);
            }
            return;
        }
        if (!orgObject->structure()->inTransitionMode()) {
            goto GiveUp;
        }

        auto result = orgObject->get(state, ObjectPropertyName(state, code->m_propertyName));
        if (!result.hasValue() || !result.isDataProperty()) {
            goto GiveUp;
        }

        newItem.m_cachedHiddenClassChainData[newItem.m_cachedhiddenClassChainLength] = orgObject->structure();

        if (code->m_inlineCacheProtoTraverseMaxIndex == 0) {
            // convert simple case to complex case
            for (size_t i = 1; i < inlineCache->m_cache.size(); i++) {
                auto oldClass = inlineCache->m_cache[i].m_cachedHiddenClass;
                block->m_inlineCacheDataSize += sizeof(size_t);
                currentCodeSizeTotal += sizeof(size_t);
                ASSERT(inlineCache->m_cache[i].m_cachedhiddenClassChainLength == 1);
                inlineCache->m_cache[i].m_cachedHiddenClassChainData = (ObjectStructure**)GC_MALLOC(sizeof(ObjectStructure*));
                inlineCache->m_cache[i].m_cachedHiddenClassChainData[0] = oldClass;
            }
        }
        code->m_inlineCacheProtoTraverseMaxIndex = std::max(cachedhiddenClassChain.size() - 1, (size_t)code->m_inlineCacheProtoTraverseMaxIndex);
    }

    return;

GiveUp:
    inlineCache->m_cache.clear();
    code->m_inlineCache = nullptr;
    code->m_missCount = maxCacheMissCount + 1;
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
    auto& records = *ctx->globalDeclarativeRecord();
    AtomicString name = slot->m_propertyName;
    auto siz = records.size();

    for (size_t i = 0; i < siz; i++) {
        if (records[i].m_name == name) {
            slot->m_lexicalIndexCache = i;
            slot->m_cachedAddress = nullptr;
            slot->m_cachedStructure = nullptr;
            auto v = (*state.context()->globalDeclarativeStorage())[i];
            if (UNLIKELY(v.isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            return v;
        }
    }

    auto findResult = go->structure()->findProperty(slot->m_propertyName);
    if (UNLIKELY(findResult.first == SIZE_MAX)) {
        ObjectGetResult res = go->get(state, ObjectPropertyName(state, slot->m_propertyName));
        if (res.hasValue()) {
            return res.value(state, go);
        } else {
            if (UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
                Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, name.string());
                if (!virtialIdResult.isEmpty())
                    return virtialIdResult;
            }
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);
            ASSERT_NOT_REACHED();
            return Value(Value::EmptyValue);
        }
    } else {
        const ObjectStructureItem* item = findResult.second.value();
        if (!item->m_descriptor.isPlainDataProperty() || !item->m_descriptor.isWritable()) {
            slot->m_cachedStructure = nullptr;
            slot->m_cachedAddress = nullptr;
            slot->m_lexicalIndexCache = std::numeric_limits<size_t>::max();
            return go->getOwnPropertyUtilForObject(state, findResult.first, go);
        }

        slot->m_cachedAddress = &go->m_values.data()[findResult.first];
        slot->m_cachedStructure = go->structure();
        slot->m_lexicalIndexCache = siz;
        return *((ObjectPropertyValue*)slot->m_cachedAddress);
    }
}

NEVER_INLINE void ByteCodeInterpreter::setGlobalVariableSlowCase(ExecutionState& state, Object* go, GlobalVariableAccessCacheItem* slot, const Value& value, ByteCodeBlock* block)
{
    Context* ctx = state.context();
    auto& records = *ctx->globalDeclarativeRecord();
    AtomicString name = slot->m_propertyName;
    auto siz = records.size();
    for (size_t i = 0; i < siz; i++) {
        if (records[i].m_name == name) {
            slot->m_lexicalIndexCache = i;
            slot->m_cachedAddress = nullptr;
            slot->m_cachedStructure = nullptr;
            auto& place = (*ctx->globalDeclarativeStorage())[i];
            if (UNLIKELY(place.isEmpty())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, ErrorObject::Messages::IsNotInitialized);
            }
            if (UNLIKELY(!records[i].m_isMutable)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::AssignmentToConstantVariable, name);
            }
            place = value;
            return;
        }
    }


    auto findResult = go->structure()->findProperty(slot->m_propertyName);
    if (UNLIKELY(findResult.first == SIZE_MAX)) {
        if (UNLIKELY(state.inStrictMode())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, slot->m_propertyName.string(), false, String::emptyString, ErrorObject::Messages::IsNotDefined);
        }
        VirtualIdDisabler d(state.context());
        go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, slot->m_propertyName), value, go);
    } else {
        const ObjectStructureItem* item = findResult.second.value();
        if (!item->m_descriptor.isPlainDataProperty() || !item->m_descriptor.isWritable()) {
            slot->m_cachedStructure = nullptr;
            slot->m_cachedAddress = nullptr;
            slot->m_lexicalIndexCache = std::numeric_limits<size_t>::max();
            go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, slot->m_propertyName), value, go);
            return;
        }

        slot->m_cachedAddress = &go->m_values.data()[findResult.first];
        slot->m_cachedStructure = go->structure();
        slot->m_lexicalIndexCache = siz;

        go->setOwnPropertyThrowsExceptionWhenStrictMode(state, findResult.first, value, go);
    }
}

NEVER_INLINE void ByteCodeInterpreter::initializeGlobalVariable(ExecutionState& state, InitializeGlobalVariable* code, const Value& value)
{
    Context* ctx = state.context();
    auto& records = *ctx->globalDeclarativeRecord();
    for (size_t i = 0; i < records.size(); i++) {
        if (records[i].m_name == code->m_variableName) {
            state.context()->globalDeclarativeStorage()->at(i) = value;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

NEVER_INLINE void ByteCodeInterpreter::createFunctionOperation(ExecutionState& state, CreateFunction* code, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    InterpretedCodeBlock* cb = code->m_codeBlock;

    LexicalEnvironment* outerLexicalEnvironment = state.mostNearestHeapAllocatedLexicalEnvironment().unwrap();

    bool isGenerator = cb->isGenerator();
    bool isAsync = cb->isAsync();
    Context* functionRealm = cb->context();
    Object* proto = functionRealm->globalObject()->functionPrototype();

    if (UNLIKELY(isGenerator && isAsync)) {
        proto = functionRealm->globalObject()->asyncGenerator();
        Value thisValue = cb->isArrowFunctionExpression() ? registerFile[byteCodeBlock->m_requiredOperandRegisterNumber] : Value(Value::EmptyValue);
        Object* homeObject = (cb->isObjectMethod() || cb->isClassMethod() || cb->isClassStaticMethod()) ? registerFile[code->m_homeObjectRegisterIndex].asObject() : nullptr;
        registerFile[code->m_registerIndex] = new ScriptAsyncGeneratorFunctionObject(state, proto, cb, outerLexicalEnvironment, thisValue, homeObject);
    } else if (UNLIKELY(isGenerator)) {
        proto = functionRealm->globalObject()->generator();
        ASSERT(!cb->isArrowFunctionExpression());
        Value thisValue(Value::EmptyValue);
        Object* homeObject = (cb->isObjectMethod() || cb->isClassMethod() || cb->isClassStaticMethod()) ? registerFile[code->m_homeObjectRegisterIndex].asObject() : nullptr;
        registerFile[code->m_registerIndex] = new ScriptGeneratorFunctionObject(state, proto, cb, outerLexicalEnvironment, thisValue, homeObject);
    } else if (UNLIKELY(isAsync)) {
        proto = functionRealm->globalObject()->asyncFunctionPrototype();
        Value thisValue = cb->isArrowFunctionExpression() ? registerFile[code->m_homeObjectRegisterIndex] : Value(Value::EmptyValue);
        Object* homeObject = (cb->isObjectMethod() || cb->isClassMethod() || cb->isClassStaticMethod()) ? registerFile[code->m_homeObjectRegisterIndex].asObject() : nullptr;
        registerFile[code->m_registerIndex] = new ScriptAsyncFunctionObject(state, proto, cb, outerLexicalEnvironment, thisValue, homeObject);
    } else if (cb->isArrowFunctionExpression()) {
        if (UNLIKELY(cb->isOneExpressionOnlyVirtualArrowFunctionExpression())) {
            registerFile[code->m_registerIndex] = new ScriptVirtualArrowFunctionObject(state, proto, cb, outerLexicalEnvironment);
        } else {
            registerFile[code->m_registerIndex] = new ScriptArrowFunctionObject(state, proto, cb, outerLexicalEnvironment, registerFile[code->m_homeObjectRegisterIndex]);
        }
    } else if (cb->isObjectMethod() || cb->isClassMethod() || cb->isClassStaticMethod()) {
        registerFile[code->m_registerIndex] = new ScriptClassMethodFunctionObject(state, proto, cb, outerLexicalEnvironment, registerFile[code->m_homeObjectRegisterIndex].asObject());
    } else {
        registerFile[code->m_registerIndex] = new ScriptFunctionObject(state, proto, cb, outerLexicalEnvironment, true, false);
    }
}

NEVER_INLINE ArrayObject* ByteCodeInterpreter::createRestElementOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock)
{
    ASSERT(state.resolveCallee());

    ArrayObject* newArray;
    size_t parameterLen = (size_t)byteCodeBlock->m_codeBlock->parameterCount() - 1; // parameter length except the rest element
    size_t argc = state.argc();
    Value* argv = state.argv();

    if (argc > parameterLen) {
        size_t arrLen = argc - parameterLen;
        newArray = new ArrayObject(state, (uint64_t)arrLen);
        for (size_t i = 0; i < arrLen; i++) {
            newArray->setIndexedProperty(state, Value(i), argv[parameterLen + i], newArray);
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

    bool inPauserScope = state->inPauserScope();
    bool inPauserResumeProcess = code->m_isTryResumeProcess || code->m_isCatchResumeProcess || code->m_isFinallyResumeProcess;
    bool shouldUseHeapAllocatedState = inPauserScope && !inPauserResumeProcess;
    ExecutionState* newState;

    if (UNLIKELY(shouldUseHeapAllocatedState)) {
        newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
        newState->ensureRareData();
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), state, newState);
#endif /* ESCARGOT_DEBUGGER */

    if (!LIKELY(inPauserResumeProcess)) {
        if (!state->ensureRareData()->m_controlFlowRecord) {
            state->ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state->ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    }

    SandBox::StackTraceDataVector stackTraceDataVector;

    if (LIKELY(!code->m_isCatchResumeProcess && !code->m_isFinallyResumeProcess)) {
        try {
            ExecutionStateVariableChanger<void (*)(ExecutionState&, bool)> changer(*state, [](ExecutionState& state, bool in) {
                state.m_onTry = in;
            });

            size_t newPc = programCounter + sizeof(TryOperation);
            interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);
            if (newState->inExecutionStopState()) {
                return Value();
            }
            if (UNLIKELY(code->m_isTryResumeProcess && newState->parent()->inExecutionStopState())) {
                return Value();
            }
            clearStack<512>();
            if (UNLIKELY(code->m_isTryResumeProcess)) {
#ifdef ESCARGOT_DEBUGGER
                Debugger::updateStopState(state->context()->debugger(), newState, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */
                state = newState->parent();
                code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
                newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
                newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
            }
        } catch (const Value& val) {
            if (UNLIKELY(code->m_isTryResumeProcess)) {
#ifdef ESCARGOT_DEBUGGER
                Debugger::updateStopState(state->context()->debugger(), newState, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */
                state = newState->parent();
                code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
                newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
                newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
            }

            newState->context()->vmInstance()->currentSandBox()->fillStackDataIntoErrorObject(val);

#ifndef NDEBUG
            char* dumpErrorInTryCatch = getenv("DUMP_ERROR_IN_TRY_CATCH");
            if (dumpErrorInTryCatch && (strcmp(dumpErrorInTryCatch, "1") == 0)) {
                ErrorObject::StackTraceData* data = ErrorObject::StackTraceData::create(newState->context()->vmInstance()->currentSandBox());
                StringBuilder builder;
                builder.appendString("Caught error in try-catch block\n");
                data->buildStackTrace(newState->context(), builder);
                ESCARGOT_LOG_ERROR("%s\n", builder.finalize()->toUTF8StringData().data());
            }
#endif
            stackTraceDataVector = std::move(newState->context()->vmInstance()->currentSandBox()->stackTraceDataVector());
            if (!code->m_hasCatch) {
                newState->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
            } else {
                stackTraceDataVector.clear();
                registerFile[code->m_catchedValueRegisterIndex] = val;
                try {
                    ExecutionStateVariableChanger<void (*)(ExecutionState&, bool)> changer(*state, [](ExecutionState& state, bool in) {
                        state.m_onCatch = in;
                    });
                    interpret(newState, byteCodeBlock, code->m_catchPosition, registerFile);
                    if (newState->inExecutionStopState()) {
                        return Value();
                    }
                } catch (const Value& val) {
                    stackTraceDataVector = newState->context()->vmInstance()->currentSandBox()->stackTraceDataVector();
                    newState->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                }
            }
        }
    } else if (code->m_isCatchResumeProcess) {
        try {
            ExecutionStateVariableChanger<void (*)(ExecutionState&, bool)> changer(*state, [](ExecutionState& state, bool in) {
                state.m_onCatch = in;
            });
            interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, programCounter + sizeof(TryOperation)), registerFile);
            if (UNLIKELY(newState->inExecutionStopState() || newState->parent()->inExecutionStopState())) {
                return Value();
            }
            state = newState->parent();
            code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
        } catch (const Value& val) {
            state = newState->parent();
            code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
            state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
        }
    }

    if (code->m_isFinallyResumeProcess) {
        ExecutionStateVariableChanger<void (*)(ExecutionState&, bool)> changer(*state, [](ExecutionState& state, bool in) {
            state.m_onFinally = in;
        });
        interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, programCounter + sizeof(TryOperation)), registerFile);
        if (newState->inExecutionStopState() || newState->parent()->inExecutionStopState()) {
            return Value();
        }
        state = newState->parent();
        code = (TryOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    } else if (code->m_hasFinalizer) {
        if (UNLIKELY(code->m_isTryResumeProcess || code->m_isCatchResumeProcess)) {
#ifdef ESCARGOT_DEBUGGER
            Debugger::updateStopState(state->context()->debugger(), newState, ESCARGOT_DEBUGGER_ALWAYS_STOP);
#endif /* ESCARGOT_DEBUGGER */
            state = newState->parent();
            code = (TryOperation*)(codeBuffer + newState->rareData()->m_programCounterWhenItStoppedByYield);
            newState = new ExecutionState(state, state->lexicalEnvironment(), state->inStrictMode());
            newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
        }
        ExecutionStateVariableChanger<void (*)(ExecutionState&, bool)> changer(*state, [](ExecutionState& state, bool in) {
            state.m_onFinally = in;
        });
        interpret(newState, byteCodeBlock, code->m_tryCatchEndPosition, registerFile);
        if (newState->inExecutionStopState()) {
            return Value();
        }
    }

    clearStack<512>();

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), newState, state);
#endif /* ESCARGOT_DEBUGGER */

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
            state->context()->vmInstance()->currentSandBox()->rethrowPreviouslyCaughtException(*state, record->value(), stackTraceDataVector);
            ASSERT_NOT_REACHED();
            // never get here. but I add return statement for removing compile warning
            return Value(Value::EmptyValue);
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

NEVER_INLINE void ByteCodeInterpreter::initializeClassOperation(ExecutionState& state, InitializeClass* code, Value* registerFile)
{
    if (code->m_stage == InitializeClass::CreateClass) {
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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Class_Extends_Value_Is_Not_Object_Nor_Null);
            } else {
                if (superClass.isObject() && superClass.asObject()->isGeneratorObject()) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Class_Prototype_Is_Not_Object_Nor_Null);
                }

                protoParent = superClass.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().prototype)).value(state, Value());

                if (!protoParent.isObject() && !protoParent.isNull()) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Class_Prototype_Is_Not_Object_Nor_Null);
                }

                constructorParent = superClass;
            }
        }

        constructorParent.asObject()->markAsPrototypeObject(state);

        ScriptClassConstructorPrototypeObject* proto = new ScriptClassConstructorPrototypeObject(state);
        proto->setPrototype(state, protoParent);

        ScriptClassConstructorFunctionObject* constructor;

        Optional<AtomicString> name;
        if (code->m_name) {
            name = AtomicString(state, code->m_name);
        }

        Optional<Object*> outerClassConstructor;
        auto home = state.mostNearestHomeObject();
        if (home) {
            outerClassConstructor = ExecutionState::convertHomeObjectIntoPrivateMemberContextObject(home.value());
        }

        if (code->m_codeBlock) {
            constructor = new ScriptClassConstructorFunctionObject(state, constructorParent.asObject(), code->m_codeBlock,
                                                                   state.mostNearestHeapAllocatedLexicalEnvironment().unwrap(), proto,
                                                                   outerClassConstructor, code->m_classSrc, name);
        } else {
            if (!heritagePresent) {
                Value argv[] = { String::emptyString, String::emptyString };
                auto functionSource = FunctionObject::createDynamicFunctionScript(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false, false, false, true);
                functionSource.codeBlock->setAsClassConstructor();
                constructor = new ScriptClassConstructorFunctionObject(state, constructorParent.asObject(),
                                                                       functionSource.codeBlock, functionSource.outerEnvironment, proto,
                                                                       outerClassConstructor, code->m_classSrc, name);
            } else {
                Value argv[] = { state.context()->staticStrings().lazyDotDotDotArgs().string(),
                                 state.context()->staticStrings().lazySuperDotDotDotArgs().string() };
                auto functionSource = FunctionObject::createDynamicFunctionScript(state, state.context()->staticStrings().constructor, 1, &argv[0], argv[1], true, false, false, true, true);
                functionSource.codeBlock->setAsClassConstructor();
                functionSource.codeBlock->setAsDerivedClassConstructor();
                constructor = new ScriptClassConstructorFunctionObject(state, constructorParent.asObject(),
                                                                       functionSource.codeBlock, functionSource.outerEnvironment, proto,
                                                                       outerClassConstructor, code->m_classSrc, name);
            }
        }

        constructor->asFunctionObject()->setFunctionPrototype(state, proto);

        // Perform CreateMethodProperty(proto, "constructor", F).
        // --> CreateMethodProperty: Let newDesc be the PropertyDescriptor{[[Value]]: V, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}.
        proto->defineOwnProperty(state, state.context()->staticStrings().constructor, ObjectPropertyDescriptor(constructor, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ValuePresent)));

        registerFile[code->m_classConstructorRegisterIndex] = constructor;
        registerFile[code->m_classPrototypeRegisterIndex] = proto;
    } else {
        auto classConstructor = registerFile[code->m_classConstructorRegisterIndex].asFunction()->asScriptClassConstructorFunctionObject();
        if (code->m_stage == InitializeClass::SetFieldSize) {
            classConstructor->m_instanceFieldInitData.resize(code->m_fieldSize);
            classConstructor->m_staticFieldInitData.resize(0, code->m_staticFieldSize);
        } else if (code->m_stage == InitializeClass::InitField) {
            registerFile[code->m_propertyInitRegisterIndex] = registerFile[code->m_propertyInitRegisterIndex].toPropertyKey(state);
            std::get<0>(classConstructor->m_instanceFieldInitData[code->m_initFieldIndex]) = registerFile[code->m_propertyInitRegisterIndex];
            std::get<2>(classConstructor->m_instanceFieldInitData[code->m_initFieldIndex]) = ScriptClassConstructorFunctionObject::NotPrivate;
        } else if (code->m_stage == InitializeClass::InitPrivateField) {
            registerFile[code->m_privatePropertyInitRegisterIndex] = registerFile[code->m_privatePropertyInitRegisterIndex].toPropertyKey(state);
            std::get<0>(classConstructor->m_instanceFieldInitData[code->m_initPrivateFieldIndex]) = registerFile[code->m_privatePropertyInitRegisterIndex];
            std::get<2>(classConstructor->m_instanceFieldInitData[code->m_initPrivateFieldIndex]) = code->m_initPrivateFieldType;
        } else if (code->m_stage == InitializeClass::SetFieldData) {
            std::get<1>(classConstructor->m_instanceFieldInitData[code->m_setFieldIndex]) = registerFile[code->m_propertySetRegisterIndex];
        } else if (code->m_stage == InitializeClass::SetPrivateFieldData) {
            std::get<1>(classConstructor->m_instanceFieldInitData[code->m_setPrivateFieldIndex]) = registerFile[code->m_privatePropertySetRegisterIndex];
        } else if (code->m_stage == InitializeClass::InitStaticField) {
            std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticFieldInitIndex]) = registerFile[code->m_staticPropertyInitRegisterIndex];
        } else if (code->m_stage == InitializeClass::InitStaticPrivateField) {
            std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticPrivateFieldInitIndex]) = registerFile[code->m_staticPrivatePropertyInitRegisterIndex];
            std::get<1>(classConstructor->m_staticFieldInitData[code->m_staticPrivateFieldInitIndex]) = ScriptClassConstructorFunctionObject::PrivateFieldValue;
        } else if (code->m_stage == InitializeClass::SetStaticFieldData) {
            Value v = registerFile[code->m_staticPropertySetRegisterIndex];
            if (!v.isUndefined()) {
                v = v.asPointerValue()->asScriptVirtualArrowFunctionObject()->call(state, Value(classConstructor), classConstructor);
            }
            classConstructor->defineOwnPropertyThrowsException(state,
                                                               ObjectPropertyName(state, std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticFieldSetIndex])),
                                                               ObjectPropertyDescriptor(v, ObjectPropertyDescriptor::AllPresent));
        } else if (code->m_stage == InitializeClass::SetStaticPrivateFieldData) {
            Value v = registerFile[code->m_staticPrivatePropertySetRegisterIndex];
            auto type = code->m_setStaticPrivateFieldType;
            if (type == ScriptClassConstructorFunctionObject::PrivateFieldValue && !v.isUndefined()) {
                v = v.asPointerValue()->asScriptVirtualArrowFunctionObject()->call(state, Value(classConstructor), classConstructor);
            }
            bool isGetter = type == ScriptClassConstructorFunctionObject::PrivateFieldGetter;
            bool isSetter = type == ScriptClassConstructorFunctionObject::PrivateFieldSetter;
            Object* contextObject = classConstructor->asScriptClassConstructorFunctionObject();

            if (isGetter || isSetter) {
                classConstructor->addPrivateAccessor(state, contextObject, AtomicString(state, Value(std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticPrivateFieldSetIndex])).asString()), v.asFunction(), isGetter, isSetter);
            } else if (type == ScriptClassConstructorFunctionObject::PrivateFieldMethod) {
                classConstructor->addPrivateMethod(state, contextObject, AtomicString(state, Value(std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticPrivateFieldSetIndex])).asString()), v.asFunction());
            } else {
                classConstructor->addPrivateField(state, contextObject, AtomicString(state, Value(std::get<0>(classConstructor->m_staticFieldInitData[code->m_staticPrivateFieldSetIndex])).asString()), v);
            }
        } else {
            ASSERT(code->m_stage == InitializeClass::CleanupStaticData);
            classConstructor->m_staticFieldInitData.clear();
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::superOperation(ExecutionState& state, SuperReference* code, Value* registerFile)
{
    if (code->m_isCall) {
        // Let newTarget be GetNewTarget().
        Object* newTarget = state.getNewTarget();
        // If newTarget is undefined, throw a ReferenceError exception.
        if (!newTarget) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::New_Target_Is_Undefined);
        }
        registerFile[code->m_dstIndex] = state.getSuperConstructor();
    } else {
        registerFile[code->m_dstIndex] = state.makeSuperPropertyReference();
    }
}

NEVER_INLINE void ByteCodeInterpreter::complexSetObjectOperation(ExecutionState& state, ComplexSetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    if (code->m_type == ComplexSetObjectOperation::Super) {
        // find `this` value for receiver
        Value thisValue(Value::ForceUninitialized);
        if (byteCodeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
            EnvironmentRecord* envRec = state.getThisEnvironment();
            ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
            thisValue = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state);
        } else {
            thisValue = registerFile[byteCodeBlock->m_requiredOperandRegisterNumber];
        }

        const Value& object = registerFile[code->m_objectRegisterIndex];
        // PutValue
        // [...]
        // Else if IsPropertyReference(V) is true, then
        //    If HasPrimitiveBase(V) is true, then
        //    [...]
        //    Let succeeded be ? base.[[Set]](GetReferencedName(V), W, GetThisValue(V)).
        //    If succeeded is false and IsStrictReference(V) is true, throw a TypeError exception.
        bool result = object.toObject(state)->set(state, ObjectPropertyName(state, registerFile[code->m_propertyNameIndex]), registerFile[code->m_loadRegisterIndex], thisValue);
        if (UNLIKELY(!result)) {
            // testing is strict mode || IsStrictReference(V)
            // IsStrictReference returns true if code is class method
            if (state.inStrictMode() || !state.resolveCallee()->codeBlock()->asInterpretedCodeBlock()->isObjectMethod()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, ObjectPropertyName(state, registerFile[code->m_propertyNameIndex]).toExceptionString(), false, String::emptyString, ErrorObject::Messages::DefineProperty_NotWritable);
            }
        }
    } else {
        ASSERT(code->m_type == ComplexSetObjectOperation::Private || code->m_type == ComplexSetObjectOperation::PrivateWithoutOuterClass);
        const Value& object = registerFile[code->m_objectRegisterIndex];
        object.toObject(state)->setPrivateMember(state, state.findPrivateMemberContextObject(), code->m_propertyName, registerFile[code->m_loadRegisterIndex],
                                                 code->m_type == ComplexSetObjectOperation::Private);
    }
}

NEVER_INLINE void ByteCodeInterpreter::complexGetObjectOperation(ExecutionState& state, ComplexGetObjectOperation* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    if (code->m_type == ComplexGetObjectOperation::Super) {
        // find `this` value for receiver
        Value thisValue(Value::ForceUninitialized);
        if (byteCodeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
            EnvironmentRecord* envRec = state.getThisEnvironment();
            ASSERT(envRec->isDeclarativeEnvironmentRecord() && envRec->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
            thisValue = envRec->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->getThisBinding(state);
        } else {
            thisValue = registerFile[byteCodeBlock->m_requiredOperandRegisterNumber];
        }

        Value object = registerFile[code->m_objectRegisterIndex];
        registerFile[code->m_storeRegisterIndex] = object.toObject(state)->get(state, ObjectPropertyName(state, registerFile[code->m_propertyNameIndex])).value(state, thisValue);
    } else {
        ASSERT(code->m_type == ComplexGetObjectOperation::Private || code->m_type == ComplexGetObjectOperation::PrivateWithoutOuterClass);
        const Value& object = registerFile[code->m_objectRegisterIndex];
        registerFile[code->m_storeRegisterIndex] = object.toObject(state)->getPrivateMember(state, state.findPrivateMemberContextObject(), code->m_propertyName,
                                                                                            code->m_type == ComplexGetObjectOperation::Private);
    }
}

NEVER_INLINE Value ByteCodeInterpreter::openLexicalEnvironment(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    OpenLexicalEnvironment* code = (OpenLexicalEnvironment*)programCounter;
    bool inWithStatement = code->m_kind == OpenLexicalEnvironment::WithStatement;

    ExecutionState* newState = nullptr;

    if (LIKELY(inWithStatement)) {
        // with statement case
        LexicalEnvironment* env = state->lexicalEnvironment();
        if (!state->ensureRareData()->m_controlFlowRecord) {
            state->ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state->ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        size_t newPc = programCounter + sizeof(OpenLexicalEnvironment);
        char* codeBuffer = byteCodeBlock->m_code.data();

        // setup new env
        EnvironmentRecord* newRecord = new ObjectEnvironmentRecord(registerFile[code->m_withOrThisRegisterIndex].toObject(*state));
        LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);

        newState = new ExecutionState(state, newEnv, state->inStrictMode());
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    } else {
        // resume execution case
        ASSERT(code->m_kind == OpenLexicalEnvironment::ResumeExecution);
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, nullptr, state->inStrictMode());
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), state, newState);
#endif /* ESCARGOT_DEBUGGER */

    size_t newPc = programCounter + sizeof(OpenLexicalEnvironment);
    char* codeBuffer = byteCodeBlock->m_code.data();

    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);

    if (newState->inExecutionStopState() || (!inWithStatement && newState->parent()->inExecutionStopState())) {
        return Value();
    }

    if (UNLIKELY(!inWithStatement)) {
        state = newState->parent();
        code = (OpenLexicalEnvironment*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), newState, state);
#endif /* ESCARGOT_DEBUGGER */

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
        programCounter = jumpTo(codeBuffer, code->m_endPostion);
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
    bool inPauserResumeProcess = code->m_blockInfo == nullptr;
    bool shouldUseIndexedStorage = byteCodeBlock->m_codeBlock->canUseIndexedVariableStorage();
    bool inPauserScope = state->inPauserScope();

    if (LIKELY(!inPauserResumeProcess)) {
        ASSERT(code->m_blockInfo->m_shouldAllocateEnvironment);
        if (LIKELY(shouldUseIndexedStorage)) {
            newRecord = new DeclarativeEnvironmentRecordIndexed(*state, code->m_blockInfo);
        } else {
            newRecord = new DeclarativeEnvironmentRecordNotIndexed(*state, false, code->m_blockInfo->m_nodeType == ASTNodeType::CatchClause);

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

    bool shouldUseHeapAllocatedState = inPauserScope && !inPauserResumeProcess;
    ExecutionState* newState;
    if (UNLIKELY(shouldUseHeapAllocatedState)) {
        newState = new ExecutionState(state, newEnv, state->inStrictMode());
        newState->ensureRareData();
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(state, newEnv, state->inStrictMode());
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), state, newState);
#endif /* ESCARGOT_DEBUGGER */

    if (!LIKELY(inPauserResumeProcess)) {
        newState->ensureRareData()->m_controlFlowRecord = state->rareData()->m_controlFlowRecord;
    }

    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);
    if (newState->inExecutionStopState() || (inPauserResumeProcess && newState->parent()->inExecutionStopState())) {
        return Value();
    }

    if (UNLIKELY(inPauserResumeProcess)) {
        state = newState->parent();
        code = (BlockOperation*)(byteCodeBlock->m_code.data() + newState->rareData()->m_programCounterWhenItStoppedByYield);
    }

#ifdef ESCARGOT_DEBUGGER
    Debugger::updateStopState(state->context()->debugger(), newState, state);
#endif /* ESCARGOT_DEBUGGER */

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

NEVER_INLINE Value ByteCodeInterpreter::constructOperation(ExecutionState& state, const Value& constructor, const size_t argc, Value* argv)
{
    if (!constructor.isConstructor()) {
        if (constructor.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Not_Constructor_Function, constructor.asFunction()->codeBlock()->functionName());
        }
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Not_Constructor);
    }

    return constructor.asPointerValue()->construct(state, argc, argv, constructor.asObject());
}

static Value callDynamicImportResolved(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    Script::ModuleData::ModulePromiseObject* promise = self->internalSlotAsPointer<Script::ModuleData::ModulePromiseObject>(0);
    Script::ModuleData::ModulePromiseObject* outerPromise = (Script::ModuleData::ModulePromiseObject*)argv[0].asPointerValue()->asPromiseObject();

    SandBox sb(state.context());
    auto result = sb.run([](ExecutionState& state, void* data) -> Value {
        Script* s = (Script*)data;
        if (!s->isExecuted()) {
            s->execute(state);
        }
        return Value(s->getModuleNamespace(state));
    },
                         outerPromise->m_loadedScript);

    if (result.error.isEmpty()) {
        if (outerPromise->m_loadedScript->moduleData()->m_asyncEvaluating) {
            outerPromise->m_loadedScript->moduleData()->m_asyncPendingPromises.pushBack(promise);
        } else {
            promise->fulfill(state, result.result);
        }
    } else {
        promise->reject(state, result.error);
    }

    return Value();
}

static Value callDynamicImportRejected(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    Script::ModuleData::ModulePromiseObject* promise = self->internalSlotAsPointer<Script::ModuleData::ModulePromiseObject>(0);
    Script::ModuleData::ModulePromiseObject* outerPromise = (Script::ModuleData::ModulePromiseObject*)argv[0].asPointerValue()->asPromiseObject();
    promise->reject(state, outerPromise->m_value);
    return Value();
}

NEVER_INLINE void ByteCodeInterpreter::callFunctionComplexCase(ExecutionState& state, CallFunctionComplexCase* code, Value* registerFile, ByteCodeBlock* byteCodeBlock)
{
    switch (code->m_kind) {
    case CallFunctionComplexCase::InWithScope: {
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
            receiverObj = state.context()->globalObjectProxy();
        }

        if (code->m_isOptional && callee.isUndefinedOrNull()) {
            registerFile[code->m_resultIndex] = Value();
        } else {
            if (code->m_hasSpreadElement) {
                ValueVector spreadArgs;
                spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
                registerFile[code->m_resultIndex] = Object::call(state, callee, receiverObj, spreadArgs.size(), spreadArgs.data());
            } else {
                registerFile[code->m_resultIndex] = Object::call(state, callee, receiverObj, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            }
        }
        break;
    }
    case CallFunctionComplexCase::MayBuiltinApply: {
        ASSERT(!code->m_isOptional);
        const Value& callee = registerFile[code->m_calleeIndex];
        const Value& receiver = registerFile[code->m_receiverOrThisIndex];
        if (UNLIKELY(!callee.isPointerValue() || !receiver.isPointerValue())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
        }

        auto functionRecord = state.mostNearestFunctionLexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();

        if (callee.asPointerValue() == state.context()->globalObject()->functionApply()) {
            if (!functionRecord->argumentsObject()) {
                Value* v = ALLOCA(sizeof(Value) * state.argc(), Value, state);
                memcpy(v, state.argv(), sizeof(Value) * state.argc());
                registerFile[code->m_resultIndex] = receiver.asPointerValue()->call(state, registerFile[code->m_argumentsStartIndex], state.argc(), v);
                return;
            }
        }

        ensureArgumentsObjectOperation(state, byteCodeBlock, registerFile);

        const auto& idInfo = byteCodeBlock->m_codeBlock->identifierInfos();
        AtomicString argumentsAtomicString = state.context()->staticStrings().arguments;
        Value argumentsValue;
        for (size_t i = 0; i < idInfo.size(); i++) {
            if (idInfo[i].m_name == argumentsAtomicString) {
                if (idInfo[i].m_needToAllocateOnStack) {
                    argumentsValue = registerFile[idInfo[i].m_indexForIndexedStorage + byteCodeBlock->m_requiredOperandRegisterNumber];
                } else {
                    argumentsValue = functionRecord->getBindingValue(state, argumentsAtomicString).m_value;
                }
            }
        }
        Value argv[2] = { registerFile[code->m_argumentsStartIndex], argumentsValue };
        registerFile[code->m_resultIndex] = callee.asPointerValue()->call(state, registerFile[code->m_receiverOrThisIndex], 2, argv);
        break;
    }
    case CallFunctionComplexCase::MayBuiltinEval: {
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

        Value eval = registerFile[code->m_calleeIndex];
        if (code->m_isOptional && eval.isUndefinedOrNull()) {
            registerFile[code->m_resultIndex] = Value();
        } else {
            if (eval.equalsTo(state, state.context()->globalObject()->eval())) {
                // do eval
                Value arg;
                if (argc) {
                    arg = argv[0];
                }

                registerFile[code->m_resultIndex] = state.context()->globalObject()->evalLocal(state, arg, registerFile[code->m_receiverOrThisIndex],
                                                                                               byteCodeBlock->m_codeBlock->asInterpretedCodeBlock(),
                                                                                               code->m_inWithScope);
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
                    if (env && env->record()->isObjectEnvironmentRecord()) {
                        thisValue = env->record()->asObjectEnvironmentRecord()->bindingObject();
                    }
                }
                registerFile[code->m_resultIndex] = Object::call(state, eval, thisValue, argc, argv);
            }
        }
        break;
    }
    case CallFunctionComplexCase::WithSpreadElement: {
        ASSERT(!code->m_isOptional);
        const Value& callee = registerFile[code->m_calleeIndex];
        const Value& receiver = code->m_receiverOrThisIndex == REGISTER_LIMIT ? Value() : registerFile[code->m_receiverOrThisIndex];

        // if PointerValue is not callable, PointerValue::call function throws builtin error
        // https://www.ecma-international.org/ecma-262/6.0/#sec-call
        // If IsCallable(F) is false, throw a TypeError exception.
        if (UNLIKELY(!callee.isPointerValue())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::NOT_Callable);
        }

        {
            ValueVector spreadArgs;
            spreadFunctionArguments(state, &registerFile[code->m_argumentsStartIndex], code->m_argumentCount, spreadArgs);
            // Return F.[[Call]](V, argumentsList).
            registerFile[code->m_resultIndex] = callee.asPointerValue()->call(state, receiver, spreadArgs.size(), spreadArgs.data());
        }
        break;
    }
    case CallFunctionComplexCase::Super: {
        ASSERT(!code->m_isOptional);
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
        FunctionEnvironmentRecord* r = thisER->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
        r->bindThisValue(state, result);
        r->functionObject()->asScriptClassConstructorFunctionObject()->initInstanceFieldMembers(state, result.asObject());
        registerFile[code->m_resultIndex] = result;
        break;
    }
    case CallFunctionComplexCase::Import: {
        // https://www.ecma-international.org/ecma-262/#sec-import-calls
        // Let referencingScriptOrModule be ! GetActiveScriptOrModule().
        Script* referencingScriptOrModule = byteCodeBlock->m_codeBlock->script();

        while (referencingScriptOrModule->topCodeBlock()->isEvalCode() || referencingScriptOrModule->topCodeBlock()->isEvalCodeInFunction()) {
            referencingScriptOrModule = referencingScriptOrModule->topCodeBlock()->parent()->script();
        }

        // Let argRef be the result of evaluating AssignmentExpression.
        // Let specifier be ? GetValue(argRef).
        const Value& specifier = registerFile[code->m_argumentsStartIndex];
        // Let promiseCapability be ! NewPromiseCapability(%Promise%).
        auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());

        // Let specifierString be ToString(specifier).
        String* specifierString = String::emptyString;
        // IfAbruptRejectPromise(specifierString, promiseCapability).
        try {
            specifierString = specifier.toString(state);
        } catch (const Value& v) {
            Value thrownValue = v;
            // If value is an abrupt completion,
            // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
            Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
            // Return capability.[[Promise]].
            registerFile[code->m_resultIndex] = promiseCapability.m_promise;
            break;
        }

        // Perform ! HostImportModuleDynamically(referencingScriptOrModule, specifierString, promiseCapability).
        // + https://tc39.es/proposal-top-level-await/#sec-finishdynamicimport
        // Let stepsFulfilled be the steps of a CallDynamicImportResolved function as specified below.
        // Let onFulfilled be CreateBuiltinFunction(stepsFulfilled, « »).
        // Let stepsRejected be the steps of a CallDynamicImportRejected function as specified below.
        // Let onRejected be CreateBuiltinFunction(stepsRejected, « »).
        // Perform ! PerformPromiseThen(innerPromise.[[Promise]], onFulfilled, onRejected).
        auto newModuleInnerPromise = new Script::ModuleData::ModulePromiseObject(state, state.context()->globalObject()->promisePrototype());
        auto innerPromiseCapability = newModuleInnerPromise->createResolvingFunctions(state);

        ExtendedNativeFunctionObject* onFulfilled = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), callDynamicImportResolved, 1));
        onFulfilled->setInternalSlotAsPointer(0, promiseCapability.m_promise);
        ExtendedNativeFunctionObject* onRejected = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), callDynamicImportRejected, 1));
        onRejected->setInternalSlotAsPointer(0, promiseCapability.m_promise);
        // Perform ! PerformPromiseThen(capability.[[Promise]], onFulfilled, onRejected).
        innerPromiseCapability.m_promise->asPromiseObject()->then(state, onFulfilled, onRejected);

        Global::platform()->hostImportModuleDynamically(byteCodeBlock->m_codeBlock->context(),
                                                        referencingScriptOrModule, specifierString, innerPromiseCapability.m_promise->asPromiseObject());

        // Return promiseCapability.[[Promise]].
        registerFile[code->m_resultIndex] = promiseCapability.m_promise;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

NEVER_INLINE void ByteCodeInterpreter::spreadFunctionArguments(ExecutionState& state, const Value* argv, const size_t argc, ValueVector& argVector)
{
    for (size_t i = 0; i < argc; i++) {
        Value arg = argv[i];
        if (arg.isObject() && arg.asObject()->isSpreadArray()) {
            ArrayObject* spreadArray = arg.asObject()->asArrayObject();
            ASSERT(spreadArray->isFastModeArray());
            for (size_t i = 0; i < spreadArray->arrayLength(state); i++) {
                argVector.push_back(spreadArray->m_fastModeData[i]);
            }
        } else {
            argVector.push_back(arg);
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::createEnumerateObject(ExecutionState& state, CreateEnumerateObject* code, Value* registerFile)
{
    Object* obj = registerFile[code->m_objectRegisterIndex].toObject(state);
    bool isDestruction = code->m_isDestruction;

    EnumerateObject* enumObj;
    if (isDestruction) {
        enumObj = new EnumerateObjectWithDestruction(state, obj);
    } else {
        enumObj = new EnumerateObjectWithIteration(state, obj);
    }
    registerFile[code->m_dataRegisterIndex] = Value((PointerValue*)enumObj);
}

NEVER_INLINE void ByteCodeInterpreter::checkLastEnumerateKey(ExecutionState& state, CheckLastEnumerateKey* code, char* codeBuffer, size_t& programCounter, Value* registerFile)
{
    EnumerateObject* data = (EnumerateObject*)registerFile[code->m_registerIndex].asPointerValue();
    if (data->checkLastEnumerateKey(state)) {
        delete data;
        programCounter = jumpTo(codeBuffer, code->m_exitPosition);
    } else {
        ADD_PROGRAM_COUNTER(CheckLastEnumerateKey);
    }
}

NEVER_INLINE void ByteCodeInterpreter::markEnumerateKey(ExecutionState& state, MarkEnumerateKey* code, Value* registerFile)
{
    EnumerateObject* data = (EnumerateObject*)registerFile[code->m_dataRegisterIndex].asPointerValue();
    Value key = registerFile[code->m_keyRegisterIndex];
    bool mark = false;
    for (size_t i = 0; i < data->m_keys.size(); i++) {
        Value cachedKey = data->m_keys[i];
        if (!cachedKey.isEmpty()) {
            if (key.isString() && cachedKey.isString()) {
                mark = key.asString()->equals(cachedKey.asString());
            } else if (key.isSymbol() && cachedKey.isSymbol()) {
                mark = (key.asSymbol() == cachedKey.asSymbol());
            }
        }
        if (mark) {
            data->m_keys[i] = Value(Value::EmptyValue);
            break;
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::executionPauseOperation(ExecutionState& state, Value* registerFile, size_t& programCounter, char* codeBuffer)
{
    ExecutionPause* code = (ExecutionPause*)programCounter;
    if (code->m_reason == ExecutionPause::Yield) {
        // http://www.ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-runtime-semantics-evaluation
        auto yieldIndex = code->m_yieldData.m_yieldIndex;
        Value ret = yieldIndex == REGISTER_LIMIT ? Value() : registerFile[yieldIndex];

        if (code->m_yieldData.m_needsToWrapYieldValueWithIterResultObject) {
            ret = IteratorObject::createIterResultObject(state, ret, false);
        }

        size_t nextProgramCounter = programCounter - (size_t)codeBuffer + sizeof(ExecutionPause) + code->m_yieldData.m_tailDataLength;

        ExecutionPauser::pause(state, ret, programCounter + sizeof(ExecutionPause), code->m_yieldData.m_tailDataLength, nextProgramCounter, code->m_yieldData.m_dstIndex, code->m_yieldData.m_dstStateIndex, ExecutionPauser::PauseReason::Yield);
    } else if (code->m_reason == ExecutionPause::Await) {
        ExecutionPauser* executionPauser = state.executionPauser();
        const Value& awaitValue = registerFile[code->m_awaitData.m_awaitIndex];
        size_t nextProgramCounter = programCounter - (size_t)codeBuffer + sizeof(ExecutionPause) + code->m_awaitData.m_tailDataLength;
        size_t tailDataPosition = programCounter + sizeof(ExecutionPause);
        size_t tailDataLength = code->m_awaitData.m_tailDataLength;

        ScriptAsyncFunctionObject::awaitOperationBeforePause(state, executionPauser, awaitValue, executionPauser->sourceObject());
        ExecutionPauser::pause(state, awaitValue, tailDataPosition, tailDataLength, nextProgramCounter, code->m_awaitData.m_dstIndex, code->m_awaitData.m_dstStateIndex, ExecutionPauser::PauseReason::Await);
    } else if (code->m_reason == ExecutionPause::GeneratorsInitialize) {
        ExecutionPauser* executionPauser = state.executionPauser();
        size_t tailDataPosition = programCounter + sizeof(ExecutionPause);
        size_t nextProgramCounter = programCounter - (size_t)codeBuffer + sizeof(ExecutionPause) + code->m_asyncGeneratorInitializeData.m_tailDataLength;
        ExecutionPauser::pause(state, Value(), tailDataPosition, code->m_asyncGeneratorInitializeData.m_tailDataLength, nextProgramCounter, REGISTER_LIMIT, REGISTER_LIMIT, ExecutionPauser::PauseReason::GeneratorsInitialize);
    }
}

NEVER_INLINE Value ByteCodeInterpreter::executionResumeOperation(ExecutionState*& state, size_t& programCounter, ByteCodeBlock* byteCodeBlock)
{
    ExecutionResume* code = (ExecutionResume*)programCounter;

    bool needsReturn = code->m_needsReturn;
    bool needsThrow = code->m_needsThrow;

    // update old parent
    auto data = code->m_pauser;
    ExecutionState* orgTreePointer = data->m_executionState;
    orgTreePointer->m_inExecutionStopState = false;
    ExecutionState* tmpTreePointer = state;

    size_t pos = programCounter + sizeof(ExecutionResume);
    size_t recursiveStatementCodeStackSize = *((size_t*)pos);
    pos += sizeof(size_t) + sizeof(size_t) * recursiveStatementCodeStackSize;

    for (size_t i = 0; i < recursiveStatementCodeStackSize; i++) {
        pos -= sizeof(size_t);
        size_t codePos = *((size_t*)pos);

#ifndef NDEBUG
        if (orgTreePointer->hasRareData() && orgTreePointer->pauseSource()) {
            // this ExecutuionState must be allocated by generatorResume function;
            ASSERT(tmpTreePointer->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->interpretedCodeBlock()->isGenerator());
        }
#endif
        auto tmpTreePointerSave = tmpTreePointer;
        auto orgTreePointerSave = orgTreePointer;
        orgTreePointer = orgTreePointer->parent();
        tmpTreePointer = tmpTreePointer->parent();

        orgTreePointer->m_inExecutionStopState = false;
        tmpTreePointerSave->setParent(orgTreePointerSave->parent());
        tmpTreePointerSave->ensureRareData()->m_programCounterWhenItStoppedByYield = codePos;
    }

    state = data->m_executionState;

    // update program counter
    programCounter = data->m_byteCodePosition + (size_t)byteCodeBlock->m_code.data();

    if (data->m_registerFile == nullptr) { // released generator. return now.
        return Value();
    }

    if (state->executionPauser()->m_resumeStateIndex == REGISTER_LIMIT) {
        if (needsReturn) {
            if (state->rareData() && state->rareData()->m_controlFlowRecord && state->rareData()->m_controlFlowRecord->size()) {
                state->rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, data->m_resumeValue, state->rareData()->m_controlFlowRecord->size());
            }
            return data->m_resumeValue;
        } else if (needsThrow) {
            state->throwException(data->m_resumeValue);
        }

        return Value(Value::EmptyValue);
    } else {
        return Value(Value::EmptyValue);
    }
}

NEVER_INLINE void ByteCodeInterpreter::metaPropertyOperation(ExecutionState& state, MetaPropertyOperation* code, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    if (code->m_type == MetaPropertyOperation::NewTarget) {
        auto newTarget = state.getNewTarget();
        if (newTarget) {
            registerFile[code->m_registerIndex] = state.getNewTarget();
        } else {
            registerFile[code->m_registerIndex] = Value();
        }
    } else {
        ASSERT(code->m_type == MetaPropertyOperation::ImportMeta);
        ASSERT(byteCodeBlock->m_codeBlock->script()->isModule());

        registerFile[code->m_registerIndex] = byteCodeBlock->m_codeBlock->script()->importMetaProperty(state);
    }
}

static Value createObjectPropertyFunctionName(ExecutionState& state, const Value& name, const char* prefix)
{
    StringBuilder builder;
    if (name.isSymbol()) {
        builder.appendString(prefix);
        if (name.asSymbol()->description().hasValue()) {
            // add symbol name if it is not an empty symbol
            builder.appendString("[");
            builder.appendString(name.asSymbol()->description().value());
            builder.appendString("]");
        }
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

    if (code->m_redefineFunctionOrClassName) {
        ASSERT(value.isFunction());
        Value fnName = createObjectPropertyFunctionName(state, propertyStringOrSymbol, "");
        value.asFunction()->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(fnName));
    }

    willBeObject.asObject()->defineOwnProperty(state, ObjectPropertyName(state, propertyStringOrSymbol), ObjectPropertyDescriptor(value, code->m_presentAttribute));
}

NEVER_INLINE void ByteCodeInterpreter::objectDefineOwnPropertyWithNameOperation(ExecutionState& state, ObjectDefineOwnPropertyWithNameOperation* code, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    Object* object = registerFile[code->m_objectRegisterIndex].asObject();
    const Value& v = registerFile[code->m_loadRegisterIndex];
    // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
    if (!object->isScriptClassConstructorPrototypeObject() && (code->m_propertyName == state.context()->staticStrings().__proto__)) {
        if (v.isObject() || v.isNull()) {
            object->setPrototype(state, v);
        }
    } else {
        const size_t minCacheFillCount = 2;
        if (object->structure() == code->m_inlineCachedStructureBefore) {
            object->m_values.push_back(v, code->m_inlineCachedStructureAfter->propertyCount());
            object->m_structure = code->m_inlineCachedStructureAfter;
        } else if (code->m_missCount > minCacheFillCount) {
            // cache miss
            object->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(v, code->m_presentAttribute));
        } else {
            code->m_missCount++;
            // should we fill cache?
            if (minCacheFillCount == code->m_missCount) {
                // try to fill cache
                auto oldStructure = object->structure();
                object->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(v, code->m_presentAttribute));
                auto newStructure = object->structure();
                if (object->isInlineCacheable() && oldStructure != newStructure) {
                    byteCodeBlock->m_otherLiteralData.push_back(oldStructure);
                    byteCodeBlock->m_otherLiteralData.push_back(newStructure);
                    code->m_inlineCachedStructureBefore = oldStructure;
                    code->m_inlineCachedStructureAfter = newStructure;
                } else {
                    // failed to cache
                }
            } else {
                object->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(v, code->m_presentAttribute));
            }
        }
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
        size_t baseIndex = arr->arrayLength(state);
        size_t elementLength = code->m_count;
        for (size_t i = 0; i < code->m_count; i++) {
            if (code->m_loadRegisterIndexs[i] != REGISTER_LIMIT) {
                Value element = registerFile[code->m_loadRegisterIndexs[i]];
                if (element.isObject() && element.asObject()->isSpreadArray()) {
                    elementLength = elementLength + element.asObject()->asArrayObject()->arrayLength(state) - 1;
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
                    for (size_t spreadIndex = 0; spreadIndex < spreadArray->arrayLength(state); spreadIndex++) {
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

        size_t baseIndex = arr->arrayLength(state);
        size_t elementIndex = 0;

        for (size_t i = 0; i < count; i++) {
            if (LIKELY(loadRegisterIndexs[i] != REGISTER_LIMIT)) {
                Value element = registerFile[loadRegisterIndexs[i]];
                if (element.isObject() && element.asObject()->isSpreadArray()) {
                    ArrayObject* spreadArray = element.asObject()->asArrayObject();
                    ASSERT(spreadArray->isFastModeArray());
                    Value spreadElement;
                    for (size_t spreadIndex = 0; spreadIndex < spreadArray->arrayLength(state); spreadIndex++) {
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

    IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, registerFile[code->m_argumentIndex]);
    size_t i = 0;
    while (true) {
        auto next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            break;
        }
        Value value = IteratorObject::iteratorValue(state, next.value());
        spreadArray->setIndexedProperty(state, Value(i++), value, spreadArray);
    }
    registerFile[code->m_registerIndex] = spreadArray;
}

static void defineObjectGetterSetterOperation(ExecutionState& state, ObjectDefineGetterSetter* code, ByteCodeBlock* byteCodeBlock, Value* registerFile, Object* object)
{
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    Value pName = code->m_objectPropertyNameRegisterIndex == REGISTER_LIMIT ? fn->codeBlock()->functionName().string() : registerFile[code->m_objectPropertyNameRegisterIndex];
    Value fnName;
    if (code->m_isGetter) {
        fnName = createObjectPropertyFunctionName(state, pName, "get ");
    } else {
        fnName = createObjectPropertyFunctionName(state, pName, "set ");
    }
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(fnName));
    JSGetterSetter* gs;
    if (code->m_isGetter) {
        gs = new (alloca(sizeof(JSGetterSetter))) JSGetterSetter(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
    } else {
        gs = new (alloca(sizeof(JSGetterSetter))) JSGetterSetter(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
    }
    ObjectPropertyDescriptor desc(*gs, code->m_presentAttribute);
    object->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, pName), desc);
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectGetterSetter(ExecutionState& state, ObjectDefineGetterSetter* code, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    Object* object = registerFile[code->m_objectRegisterIndex].toObject(state);
    const size_t minCacheFillCount = 2;
    if (object->structure() == code->m_inlineCachedStructureBefore) {
        JSGetterSetter* gs;
        if (code->m_isGetter) {
            gs = new JSGetterSetter(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
        } else {
            gs = new JSGetterSetter(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
        }
        object->m_values.push_back(Value(gs), code->m_inlineCachedStructureAfter->propertyCount());
        object->m_structure = code->m_inlineCachedStructureAfter;
    } else if (code->m_missCount > minCacheFillCount) {
        // cache miss
        defineObjectGetterSetterOperation(state, code, byteCodeBlock, registerFile, object);
    } else {
        code->m_missCount++;
        // should we fill cache?
        if (minCacheFillCount == code->m_missCount && code->m_isPrecomputed) {
            // try to fill cache
            auto oldStructure = object->structure();
            defineObjectGetterSetterOperation(state, code, byteCodeBlock, registerFile, object);
            auto newStructure = object->structure();
            if (object->isInlineCacheable() && oldStructure != newStructure) {
                byteCodeBlock->m_otherLiteralData.push_back(oldStructure);
                byteCodeBlock->m_otherLiteralData.push_back(newStructure);
                code->m_inlineCachedStructureBefore = oldStructure;
                code->m_inlineCachedStructureAfter = newStructure;
            } else {
                // failed to cache
            }
        } else {
            defineObjectGetterSetterOperation(state, code, byteCodeBlock, registerFile, object);
        }
    }
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
        return incrementOperationSlowCase(state, value);
    }
}

NEVER_INLINE Value ByteCodeInterpreter::incrementOperationSlowCase(ExecutionState& state, const Value& value)
{
    // https://www.ecma-international.org/ecma-262/#sec-postfix-increment-operator
    // https://www.ecma-international.org/ecma-262/#sec-prefix-increment-operator
    auto newVal = value.toNumeric(state);
    if (UNLIKELY(newVal.second)) {
        return Value(newVal.first.asBigInt()->increment(state));
    } else {
        return Value(newVal.first.asNumber() + 1);
    }
}

ALWAYS_INLINE Value ByteCodeInterpreter::decrementOperation(ExecutionState& state, const Value& value)
{
    if (LIKELY(value.isInt32())) {
        int32_t a = value.asInt32();
        int32_t b = 1;
        int32_t c;
        bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::sub(a, b, c);
        if (LIKELY(result)) {
            return Value(c);
        } else {
            return Value(Value::EncodeAsDouble, (double)a - (double)b);
        }
    } else {
        return decrementOperationSlowCase(state, value);
    }
}

NEVER_INLINE Value ByteCodeInterpreter::decrementOperationSlowCase(ExecutionState& state, const Value& value)
{
    // https://www.ecma-international.org/ecma-262/#sec-postfix-decrement-operator
    // https://www.ecma-international.org/ecma-262/#sec-prefix-decrement-operator
    auto newVal = value.toNumeric(state);
    if (UNLIKELY(newVal.second)) {
        return Value(newVal.first.asBigInt()->decrement(state));
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
        if (p->isSymbol()) {
            val = state.context()->staticStrings().symbol.string();
        } else if (p->isBigInt()) {
            val = state.context()->staticStrings().bigint.string();
        } else {
            ASSERT(p->isObject());
            if (!p->isCallable()) {
                val = state.context()->staticStrings().object.string();
#if defined(ESCARGOT_ENABLE_TEST)
            } else if (UNLIKELY(p->asObject()->isHTMLDDA())) {
                val = state.context()->staticStrings().undefined.string();
#endif
            } else {
                val = state.context()->staticStrings().function.string();
            }
        }
    }

    registerFile[code->m_dstIndex] = val;
}

NEVER_INLINE void ByteCodeInterpreter::iteratorOperation(ExecutionState& state, size_t& programCounter, Value* registerFile, char* codeBuffer)
{
    IteratorOperation* code = (IteratorOperation*)programCounter;
    if (code->m_operation == IteratorOperation::Operation::GetIterator) {
        const Value& obj = registerFile[code->m_getIteratorData.m_srcObjectRegisterIndex];
        registerFile[code->m_getIteratorData.m_dstIteratorRecordIndex] = IteratorObject::getIterator(state, obj, code->m_getIteratorData.m_isSyncIterator);
        if (code->m_getIteratorData.m_dstIteratorObjectIndex != REGISTER_LIMIT) {
            registerFile[code->m_getIteratorData.m_dstIteratorObjectIndex] = registerFile[code->m_getIteratorData.m_dstIteratorRecordIndex].asPointerValue()->asIteratorRecord()->m_iterator;
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorClose) {
        if (code->m_iteratorCloseData.m_execeptionRegisterIndexIfExists != REGISTER_LIMIT) {
            IteratorRecord* iteratorRecord = registerFile[code->m_iteratorCloseData.m_iterRegisterIndex].asPointerValue()->asIteratorRecord();
            IteratorObject::iteratorClose(state, iteratorRecord, registerFile[code->m_iteratorCloseData.m_execeptionRegisterIndexIfExists], true);
        } else {
            bool exceptionWasThrown = state.hasRareData() && state.rareData()->m_controlFlowRecord && state.rareData()->m_controlFlowRecord->back() && state.rareData()->m_controlFlowRecord->back()->reason() == ControlFlowRecord::NeedsThrow;
            IteratorRecord* iteratorRecord = registerFile[code->m_iteratorCloseData.m_iterRegisterIndex].asPointerValue()->asIteratorRecord();
            Object* iterator = iteratorRecord->m_iterator;
            Value returnFunction = iterator->get(state, ObjectPropertyName(state.context()->staticStrings().stringReturn)).value(state, iterator);
            if (returnFunction.isUndefined()) {
                ADD_PROGRAM_COUNTER(IteratorOperation);
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

            if (exceptionWasThrown) {
                ADD_PROGRAM_COUNTER(IteratorOperation);
                return;
            }

            if (innerResultHasException) {
                state.throwException(innerResult);
            }

            if (!innerResult.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Iterator close result is not an object");
            }
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorBind) {
        auto strings = &state.context()->staticStrings();

        Optional<Object*> nextResult;
        Value value;
        IteratorRecord* iteratorRecord = registerFile[code->m_iteratorBindData.m_iterRegisterIndex].asPointerValue()->asIteratorRecord();

        if (!iteratorRecord->m_done) {
            try {
                nextResult = IteratorObject::iteratorStep(state, iteratorRecord);
            } catch (const Value& e) {
                Value exceptionValue = e;
                iteratorRecord->m_done = true;
                state.throwException(exceptionValue);
            }

            if (!nextResult.hasValue()) {
                iteratorRecord->m_done = true;
            } else {
                try {
                    value = IteratorObject::iteratorValue(state, nextResult.value());
                } catch (const Value& e) {
                    Value exceptionValue = e;
                    iteratorRecord->m_done = true;
                    state.throwException(exceptionValue);
                }
            }
        }

        registerFile[code->m_iteratorBindData.m_registerIndex] = value;
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorTestDone) {
        if (code->m_iteratorTestDoneData.m_isIteratorRecord) {
            registerFile[code->m_iteratorTestDoneData.m_dstRegisterIndex] = Value(
                registerFile[code->m_iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex].asPointerValue()->asIteratorRecord()->m_done);
        } else {
            registerFile[code->m_iteratorTestDoneData.m_dstRegisterIndex] = Value(
                registerFile[code->m_iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex].asObject()->get(state, state.context()->staticStrings().done).value(state, registerFile[code->m_iteratorTestDoneData.m_iteratorRecordOrObjectRegisterIndex]).toBoolean(state));
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorNext) {
        auto record = registerFile[code->m_iteratorNextData.m_iteratorRecordRegisterIndex].asPointerValue()->asIteratorRecord();
        if (code->m_iteratorNextData.m_valueRegisterIndex == REGISTER_LIMIT) {
            registerFile[code->m_iteratorNextData.m_returnRegisterIndex] = Object::call(state, record->m_nextMethod, record->m_iterator, 0, nullptr);
        } else {
            registerFile[code->m_iteratorNextData.m_returnRegisterIndex] = Object::call(state, record->m_nextMethod, record->m_iterator, 1, &registerFile[code->m_iteratorNextData.m_valueRegisterIndex]);
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorTestResultIsObject) {
        if (!registerFile[code->m_iteratorTestResultIsObjectData.m_valueRegisterIndex].isObject()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "IteratorResult is not an object");
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorValue) {
        registerFile[code->m_iteratorValueData.m_dstRegisterIndex] = IteratorObject::iteratorValue(state, registerFile[code->m_iteratorValueData.m_srcRegisterIndex].asObject());
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else if (code->m_operation == IteratorOperation::Operation::IteratorCheckOngoingExceptionOnAsyncIteratorClose) {
        ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
        if (record && record->reason() == ControlFlowRecord::NeedsThrow) {
            state.context()->throwException(state, record->value());
        }
        ADD_PROGRAM_COUNTER(IteratorOperation);
    } else {
        ASSERT_NOT_REACHED();
    }
}

NEVER_INLINE void ByteCodeInterpreter::getMethodOperation(ExecutionState& state, size_t programCounter, Value* registerFile)
{
    GetMethod* code = (GetMethod*)programCounter;
    registerFile[code->m_resultRegisterIndex] = Object::getMethod(state, registerFile[code->m_objectRegisterIndex], code->m_propertyName);
}

NEVER_INLINE Object* ByteCodeInterpreter::restBindOperation(ExecutionState& state, IteratorRecord* iteratorRecord)
{
    auto strings = &state.context()->staticStrings();

    Object* result = new ArrayObject(state);
    Optional<Object*> nextResult;
    Value value;
    size_t index = 0;

    while (true) {
        if (!iteratorRecord->m_done) {
            try {
                nextResult = IteratorObject::iteratorStep(state, iteratorRecord);
            } catch (const Value& e) {
                Value exceptionValue = e;
                iteratorRecord->m_done = true;
                state.throwException(exceptionValue);
            }

            if (!nextResult.hasValue()) {
                iteratorRecord->m_done = true;
            }
        }

        if (iteratorRecord->m_done) {
            break;
        }

        try {
            value = IteratorObject::iteratorValue(state, nextResult.value());
        } catch (const Value& e) {
            Value exceptionValue = e;
            iteratorRecord->m_done = true;
            state.throwException(exceptionValue);
        }

        result->setIndexedProperty(state, Value(index++), value);
    }

    return result;
}

NEVER_INLINE void ByteCodeInterpreter::taggedTemplateOperation(ExecutionState& state, size_t& programCounter, Value* registerFile, char* codeBuffer, ByteCodeBlock* byteCodeBlock)
{
    TaggedTemplateOperation* code = (TaggedTemplateOperation*)programCounter;
    InterpretedCodeBlock* cb = byteCodeBlock->m_codeBlock;
    auto& cache = cb->taggedTemplateLiteralCache();

    if (code->m_operaton == TaggedTemplateOperation::TestCacheOperation) {
        if (cache.size() > code->m_testCacheOperationData.m_cacheIndex && cache[code->m_testCacheOperationData.m_cacheIndex]) {
            registerFile[code->m_testCacheOperationData.m_registerIndex] = cache[code->m_testCacheOperationData.m_cacheIndex].value();
            programCounter = code->m_testCacheOperationData.m_jumpPosition;
        } else {
            ADD_PROGRAM_COUNTER(TaggedTemplateOperation);
        }
    } else {
        ASSERT(code->m_operaton == TaggedTemplateOperation::FillCacheOperation);
        if (cache.size() <= code->m_fillCacheOperationData.m_cacheIndex) {
            cache.resize(code->m_fillCacheOperationData.m_cacheIndex + 1);
        }
        cache[code->m_fillCacheOperationData.m_cacheIndex] = registerFile[code->m_fillCacheOperationData.m_registerIndex].asPointerValue()->asArrayObject();
        ADD_PROGRAM_COUNTER(TaggedTemplateOperation);
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
    registerFile[code->m_storeRegisterIndex] = obj->getIndexedPropertyValue(state, property, willBeObject);
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
        Object::throwCannotWriteError(state, ObjectStructurePropertyName(state, property.toString(state)));
    }
}

NEVER_INLINE void ByteCodeInterpreter::ensureArgumentsObjectOperation(ExecutionState& state, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    auto functionRecord = state.mostNearestFunctionLexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
    auto functionObject = functionRecord->functionObject()->asScriptFunctionObject();
    bool isMapped = functionObject->interpretedCodeBlock()->shouldHaveMappedArguments();
    functionObject->generateArgumentsObject(state, state.argc(), state.argv(), functionRecord, registerFile + byteCodeBlock->m_requiredOperandRegisterNumber, isMapped);
}
} // namespace Escargot
