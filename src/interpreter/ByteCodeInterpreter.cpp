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
#include "parser/ScriptParser.h"
#include "util/Util.h"
#include "../third_party/checked_arithmetic/CheckedArithmetic.h"

#if ESCARGOT_ENABLE_PROXY_REFLECT
#include "runtime/ProxyObject.h"
#endif

namespace Escargot {


static Value implicitClassConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value();
}

static Object* implicitClassConstructorCtor(ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv)
{
    return new Object(state);
}

#define ADD_PROGRAM_COUNTER(CodeType) programCounter += sizeof(CodeType);

ALWAYS_INLINE size_t jumpTo(char* codeBuffer, const size_t jumpPosition)
{
    return (size_t)&codeBuffer[jumpPosition];
}

ALWAYS_INLINE size_t resolveProgramCounter(char* codeBuffer, const size_t programCounter)
{
    return programCounter - (size_t)codeBuffer;
}

Value ByteCodeInterpreter::interpret(ExecutionState& state, ByteCodeBlock* byteCodeBlock, size_t programCounter, Value* registerFile, void* initAddressFiller)
{
#if defined(COMPILER_GCC)
    if (UNLIKELY(initAddressFiller != nullptr)) {
        *((size_t*)initAddressFiller) = ((size_t) && FillOpcodeTableOpcodeLbl);
    }
#endif

    ExecutionContext* ec = state.executionContext();
    char* codeBuffer = byteCodeBlock->m_code.data();
    programCounter = (size_t)(codeBuffer + programCounter);

    {
        try {
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
                ASSERT(code->m_registerIndex1 < (byteCodeBlock->m_requiredRegisterFileSizeInValueSize + byteCodeBlock->m_codeBlock->asInterpretedCodeBlock()->identifierOnStackCount() + 1));
                registerFile[code->m_registerIndex1] = registerFile[code->m_registerIndex0];
                ADD_PROGRAM_COUNTER(Move);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(GetGlobalObject)
                :
            {
                GetGlobalObject* code = (GetGlobalObject*)programCounter;
                GlobalObject* globalObject = state.context()->globalObject();
                if (LIKELY(globalObject->structure() == code->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= code->m_cachedAddress);
                    ASSERT(code->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    registerFile[code->m_registerIndex] = *((SmallValue*)code->m_cachedAddress);
                } else {
                    registerFile[code->m_registerIndex] = getGlobalObjectSlowCase(state, globalObject, code, byteCodeBlock);
                }
                ADD_PROGRAM_COUNTER(GetGlobalObject);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(SetGlobalObject)
                :
            {
                SetGlobalObject* code = (SetGlobalObject*)programCounter;
                GlobalObject* globalObject = state.context()->globalObject();
                if (LIKELY(globalObject->structure() == code->m_cachedStructure)) {
                    ASSERT(globalObject->m_values.data() <= code->m_cachedAddress);
                    ASSERT(code->m_cachedAddress < (globalObject->m_values.data() + globalObject->structure()->propertyCount()));
                    *((SmallValue*)code->m_cachedAddress) = registerFile[code->m_registerIndex];
                } else {
                    setGlobalObjectSlowCase(state, globalObject, code, registerFile[code->m_registerIndex], byteCodeBlock);
                }
                ADD_PROGRAM_COUNTER(SetGlobalObject);
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

            DEFINE_OPCODE(Increment)
                :
            {
                Increment* code = (Increment*)programCounter;
                const Value& val = registerFile[code->m_srcIndex];
                if (LIKELY(val.isInt32())) {
                    int32_t a = val.asInt32();
                    int32_t b = 1;
                    int32_t c;
                    bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::add(a, b, c);
                    if (LIKELY(result)) {
                        registerFile[code->m_dstIndex] = Value(c);
                    } else {
                        registerFile[code->m_dstIndex] = Value(Value::EncodeAsDouble, (double)a + (double)b);
                    }
                } else {
                    registerFile[code->m_dstIndex] = plusSlowCase(state, Value(val.toNumber(state)), Value(1));
                }
                ADD_PROGRAM_COUNTER(Increment);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(Decrement)
                :
            {
                Decrement* code = (Decrement*)programCounter;
                const Value& val = registerFile[code->m_srcIndex];
                if (LIKELY(val.isInt32())) {
                    int32_t a = val.asInt32();
                    int32_t b = -1;
                    int32_t c;
                    bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::add(a, b, c);
                    if (LIKELY(result)) {
                        registerFile[code->m_dstIndex] = Value(c);
                    } else {
                        registerFile[code->m_dstIndex] = Value(Value::EncodeAsDouble, (double)a + (double)b);
                    }
                } else {
                    registerFile[code->m_dstIndex] = Value(val.toNumber(state) - 1);
                }
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
                    if (LIKELY(arr->isFastModeArray())) {
                        uint32_t idx = property.tryToUseAsArrayIndex(state);
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
                registerFile[code->m_resultIndex] = FunctionObject::call(state, callee, Value(), code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
                ADD_PROGRAM_COUNTER(CallFunction);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(CallFunctionWithReceiver)
                :
            {
                CallFunctionWithReceiver* code = (CallFunctionWithReceiver*)programCounter;
                const Value& callee = registerFile[code->m_calleeIndex];
                const Value& receiver = registerFile[code->m_receiverIndex];
                registerFile[code->m_resultIndex] = FunctionObject::call(state, callee, receiver, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
                ADD_PROGRAM_COUNTER(CallFunctionWithReceiver);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(LoadByHeapIndex)
                :
            {
                LoadByHeapIndex* code = (LoadByHeapIndex*)programCounter;
                LexicalEnvironment* upperEnv = ec->lexicalEnvironment();
                for (size_t i = 0; i < code->m_upperIndex; i++) {
                    upperEnv = upperEnv->outerEnvironment();
                }
                FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                ASSERT(record->isFunctionEnvironmentRecordOnHeap() || record->isFunctionEnvironmentRecordNotIndexed());
                registerFile[code->m_registerIndex] = ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index];
                ADD_PROGRAM_COUNTER(LoadByHeapIndex);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(StoreByHeapIndex)
                :
            {
                StoreByHeapIndex* code = (StoreByHeapIndex*)programCounter;
                LexicalEnvironment* upperEnv = ec->lexicalEnvironment();
                for (size_t i = 0; i < code->m_upperIndex; i++) {
                    upperEnv = upperEnv->outerEnvironment();
                }
                FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                ASSERT(record->isFunctionEnvironmentRecordOnHeap() || record->isFunctionEnvironmentRecordNotIndexed());
                ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index] = registerFile[code->m_registerIndex];
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
                ArrayObject* arr = new ArrayObject(state);
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
                    willBeObject.asObject()->defineOwnProperty(state, objPropName, ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], ObjectPropertyDescriptor::AllPresent));
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
                    willBeObject.asObject()->defineOwnProperty(state, ObjectPropertyName(code->m_propertyName), ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], ObjectPropertyDescriptor::AllPresent));
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
                        if (LIKELY(code->m_loadRegisterIndexs[i] != std::numeric_limits<ByteCodeRegisterIndex>::max())) {
                            arr->m_fastModeData[i + code->m_baseIndex] = registerFile[code->m_loadRegisterIndexs[i]];
                        }
                    }
                } else {
                    for (size_t i = 0; i < code->m_count; i++) {
                        if (LIKELY(code->m_loadRegisterIndexs[i] != std::numeric_limits<ByteCodeRegisterIndex>::max())) {
                            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(i + code->m_baseIndex)), ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndexs[i]], ObjectPropertyDescriptor::AllPresent));
                        }
                    }
                }
                ADD_PROGRAM_COUNTER(ArrayDefineOwnPropertyOperation);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(NewOperation)
                :
            {
                NewOperation* code = (NewOperation*)programCounter;
                registerFile[code->m_resultIndex] = newOperation(state, registerFile[code->m_calleeIndex], code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
                ADD_PROGRAM_COUNTER(NewOperation);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(UnaryTypeof)
                :
            {
                UnaryTypeof* code = (UnaryTypeof*)programCounter;
                Value val;
                if (code->m_id.string()->length()) {
                    val = loadByName(state, ec->lexicalEnvironment(), code->m_id, false);
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
                    if (p->isFunctionObject()) {
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
                registerFile[code->m_registerIndex] = loadByName(state, ec->lexicalEnvironment(), code->m_name);
                ADD_PROGRAM_COUNTER(LoadByName);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(StoreByName)
                :
            {
                StoreByName* code = (StoreByName*)programCounter;
                storeByName(state, ec->lexicalEnvironment(), code->m_name, registerFile[code->m_registerIndex]);
                ADD_PROGRAM_COUNTER(StoreByName);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(CreateFunction)
                :
            {
                CreateFunction* code = (CreateFunction*)programCounter;
                registerFile[code->m_registerIndex] = new FunctionObject(state, code->m_codeBlock, ec->lexicalEnvironment());
                ADD_PROGRAM_COUNTER(CreateFunction);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(CreateImplicitConstructor)
                :
            {
                CreateImplicitConstructor* code = (CreateImplicitConstructor*)programCounter;
                registerFile[code->m_registerIndex] = new FunctionObject(state, NativeFunctionInfo(::Escargot::AtomicString::fromPayload(String::emptyString), implicitClassConstructor, 1, implicitClassConstructorCtor, NativeFunctionInfo::Flags::Strict | NativeFunctionInfo::Flags::Constructor | NativeFunctionInfo::Flags::ClassConstructor));
                ADD_PROGRAM_COUNTER(CreateImplicitConstructor);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(CallEvalFunction)
                :
            {
                CallEvalFunction* code = (CallEvalFunction*)programCounter;
                evalOperation(state, code, registerFile, byteCodeBlock, ec);
                ADD_PROGRAM_COUNTER(CallEvalFunction);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(TryOperation)
                :
            {
                TryOperation* code = (TryOperation*)programCounter;
                programCounter = tryOperation(state, code, ec, ec->lexicalEnvironment(), programCounter, byteCodeBlock, registerFile);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(TryCatchWithBodyEnd)
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
                Value v = withOperation(state, code, registerFile[code->m_registerIndex].toObject(state), ec, ec->lexicalEnvironment(), newPc, byteCodeBlock, registerFile, stackStorage);
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
                deleteOperation(state, ec->lexicalEnvironment(), code, registerFile);
                ADD_PROGRAM_COUNTER(UnaryDelete);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(TemplateOperation)
                :
            {
                TemplateOperation* code = (TemplateOperation*)programCounter;
                templateOperation(state, ec->lexicalEnvironment(), code, registerFile);
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
                registerFile[code->m_resultIndex] = callFunctionInWithScope(state, code, ec, ec->lexicalEnvironment(), &registerFile[code->m_argumentsStartIndex]);
                ADD_PROGRAM_COUNTER(CallFunctionInWithScope);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(DeclareFunctionDeclarations)
                :
            {
                DeclareFunctionDeclarations* code = (DeclareFunctionDeclarations*)programCounter;
                Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
                declareFunctionDeclarations(state, code, ec->lexicalEnvironment(), stackStorage);
                ADD_PROGRAM_COUNTER(DeclareFunctionDeclarations);
                NEXT_INSTRUCTION();
            }

            DEFINE_OPCODE(ReturnFunctionSlowCase)
                :
            {
                ReturnFunctionSlowCase* code = (ReturnFunctionSlowCase*)programCounter;
                Value ret;
                if (code->m_registerIndex != std::numeric_limits<ByteCodeRegisterIndex>::max()) {
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
                ErrorObject::throwBuiltinError(state, (ErrorObject::Code)code->m_errorKind, code->m_errorMessage);
            }

            DEFINE_OPCODE(End)
                :
            {
                return registerFile[0];
            }

            DEFINE_DEFAULT

        } catch (const Value& v) {
            if (byteCodeBlock->m_codeBlock->isInterpretedCodeBlock() && byteCodeBlock->m_codeBlock->asInterpretedCodeBlock()->byteCodeBlock() == nullptr) {
                byteCodeBlock->m_codeBlock->asInterpretedCodeBlock()->m_byteCodeBlock = byteCodeBlock;
            }
            processException(state, v, ec, programCounter);
        }
    }

    ASSERT_NOT_REACHED();

#if defined(COMPILER_GCC)
FillOpcodeTableOpcodeLbl:
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
            env->record()->setMutableBindingByIndex(state, result.m_index, name, value);
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
    if (UNLIKELY(left.isPointerValue() && left.asPointerValue()->isDateObject())) {
        lval = left.toPrimitive(state, Value::PreferString);
    } else {
        lval = left.toPrimitive(state);
    }

    if (UNLIKELY(right.isPointerValue() && right.asPointerValue()->isDateObject())) {
        rval = right.toPrimitive(state, Value::PreferString);
    } else {
        rval = right.toPrimitive(state);
    }
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

NEVER_INLINE Object* ByteCodeInterpreter::newOperation(ExecutionState& state, const Value& callee, size_t argc, Value* argv)
{
    if (LIKELY(callee.isFunction())) {
        return callee.asFunction()->newInstance(state, argc, argv);
    }

#if ESCARGOT_ENABLE_PROXY_REFLECT
    if (callee.isObject() && callee.asObject()->isProxyObject() && callee.asPointerValue()->asProxyObject()->isConstructor()) {
        return callee.asPointerValue()->asProxyObject()->construct(state, argc, argv);
    }
#endif

    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
    return nullptr;
}

NEVER_INLINE Value ByteCodeInterpreter::instanceOfOperation(ExecutionState& state, const Value& left, const Value& right)
{
    if (!right.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    return Value(right.asFunction()->hasInstance(state, left));
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
        lval = left.toPrimitive(state);
        rval = right.toPrimitive(state);
    } else {
        rval = right.toPrimitive(state);
        lval = left.toPrimitive(state);
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-11.8.5
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
        lval = left.toPrimitive(state);
        rval = right.toPrimitive(state);
    } else {
        rval = right.toPrimitive(state);
        lval = left.toPrimitive(state);
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
            Object* protoObject = obj->getPrototypeObject(state);
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
        obj = obj->getPrototypeObject(state);
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
                Object* o = obj->getPrototypeObject(state);
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
        auto desc = obj->structure()->readProperty(state, idx).m_descriptor;
        if (desc.isPlainDataProperty() && desc.isWritable()) {
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

    std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_ignore_off_page_allocator<String*>> keyStringSet;

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
        std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_ignore_off_page_allocator<String*>>* keyStringSet;
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
    std::vector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> oldKeys;
    if (data->m_keys.size()) {
        oldKeys.insert(oldKeys.end(), &data->m_keys[0], &data->m_keys[data->m_keys.size() - 1] + 1);
    }
    std::vector<Value, GCUtil::gc_malloc_ignore_off_page_allocator<Value>> differenceKeys;
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

NEVER_INLINE Value ByteCodeInterpreter::getGlobalObjectSlowCase(ExecutionState& state, Object* go, GetGlobalObject* code, ByteCodeBlock* block)
{
    size_t idx = go->structure()->findProperty(state, code->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        ObjectGetResult res = go->get(state, ObjectPropertyName(state, code->m_propertyName));
        if (res.hasValue()) {
            return res.value(state, go);
        } else {
            if (UNLIKELY((bool)state.context()->virtualIdentifierCallback())) {
                Value virtialIdResult = state.context()->virtualIdentifierCallback()(state, code->m_propertyName.plainString());
                if (!virtialIdResult.isEmpty())
                    return virtialIdResult;
            }
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.plainString(), false, String::emptyString, errorMessage_IsNotDefined);
        }
    } else {
        const ObjectStructureItem& item = go->structure()->readProperty(state, idx);
        if (!item.m_descriptor.isPlainDataProperty()) {
            code->m_cachedStructure = nullptr;
            return go->getOwnPropertyUtilForObject(state, idx, go);
        }

        if ((size_t)code->m_cachedAddress) {
            code->m_cachedAddress = &go->m_values.data()[idx];
            code->m_cachedStructure = go->structure();
            block->m_objectStructuresInUse->insert(go->structure());
        } else {
            code->m_cachedAddress = (void*)((size_t)1);
        }
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

NEVER_INLINE void ByteCodeInterpreter::setGlobalObjectSlowCase(ExecutionState& state, Object* go, SetGlobalObject* code, const Value& value, ByteCodeBlock* block)
{
    size_t idx = go->structure()->findProperty(state, code->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        if (UNLIKELY(state.inStrictMode())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.plainString(), false, String::emptyString, errorMessage_IsNotDefined);
        }
        VirtualIdDisabler d(state.context());
        go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, go);
    } else {
        const ObjectStructureItem& item = go->structure()->readProperty(state, idx);
        if (!item.m_descriptor.isPlainDataProperty()) {
            code->m_cachedStructure = nullptr;
            go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, go);
            return;
        }

        if ((size_t)code->m_cachedAddress) {
            code->m_cachedAddress = &go->m_values.data()[idx];
            code->m_cachedStructure = go->structure();
            block->m_objectStructuresInUse->insert(go->structure());
        } else {
            code->m_cachedAddress = (void*)((size_t)1);
        }

        go->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value, go);
    }
}

NEVER_INLINE size_t ByteCodeInterpreter::tryOperation(ExecutionState& state, TryOperation* code, ExecutionContext* ec, LexicalEnvironment* env, size_t programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile)
{
    char* codeBuffer = byteCodeBlock->m_code.data();
    try {
        if (!state.ensureRareData()->m_controlFlowRecord) {
            state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
        }
        state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
        size_t newPc = programCounter + sizeof(TryOperation);
        clearStack<386>();
        interpret(state, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile);
        programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
    } catch (const Value& val) {
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
        if (code->m_hasCatch == false) {
            state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
            programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
        } else {
            // setup new env
            EnvironmentRecord* newRecord = new DeclarativeEnvironmentRecordNotIndexedForCatch();
            newRecord->createBinding(state, code->m_catchVariableName);
            newRecord->setMutableBinding(state, code->m_catchVariableName, val);
            LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
            ExecutionContext* newEc = new ExecutionContext(state.context(), state.executionContext(), newEnv, state.inStrictMode());
            try {
                ExecutionState newState(&state, newEc);
                newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;
                clearStack<386>();
                interpret(newState, byteCodeBlock, code->m_catchPosition, registerFile);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            } catch (const Value& val) {
                state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            }
        }
    }
    return programCounter;
}

class EvalCodeBlockWithFlagSetter {
public:
    EvalCodeBlockWithFlagSetter(InterpretedCodeBlock* b, bool inWith)
        : blk(b)
    {
        inWithBefore = blk->isInWithScope();
        if (inWith)
            blk->setInWithScope();
    }

    ~EvalCodeBlockWithFlagSetter()
    {
        if (inWithBefore) {
            blk->setInWithScope();
        } else {
            blk->clearInWithScope();
        }
    }

    bool inWithBefore;
    InterpretedCodeBlock* blk;
};

NEVER_INLINE void ByteCodeInterpreter::evalOperation(ExecutionState& state, CallEvalFunction* code, Value* registerFile, ByteCodeBlock* byteCodeBlock, ExecutionContext* ec)
{
    Value eval = registerFile[code->m_evalIndex];
    if (eval.equalsTo(state, state.context()->globalObject()->eval())) {
        // do eval
        Value arg;
        if (code->m_argumentCount) {
            arg = registerFile[code->m_argumentsStartIndex];
        }
        Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
        EvalCodeBlockWithFlagSetter s(byteCodeBlock->m_codeBlock->asInterpretedCodeBlock(), code->m_inWithScope);
        registerFile[code->m_resultIndex] = state.context()->globalObject()->evalLocal(state, arg, stackStorage[0], byteCodeBlock->m_codeBlock->asInterpretedCodeBlock());
    } else {
        Value thisValue;
        if (code->m_inWithScope) {
            LexicalEnvironment* env = ec->lexicalEnvironment();
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
        registerFile[code->m_resultIndex] = FunctionObject::call(state, eval, thisValue, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
    }
}

NEVER_INLINE Value ByteCodeInterpreter::withOperation(ExecutionState& state, WithOperation* code, Object* obj, ExecutionContext* ec, LexicalEnvironment* env, size_t& programCounter, ByteCodeBlock* byteCodeBlock, Value* registerFile, Value* stackStorage)
{
    if (!state.ensureRareData()->m_controlFlowRecord) {
        state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
    }
    state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
    size_t newPc = programCounter + sizeof(WithOperation);
    char* codeBuffer = byteCodeBlock->m_code.data();

    // setup new env
    EnvironmentRecord* newRecord = new ObjectEnvironmentRecord(obj);
    LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
    ExecutionContext* newEc = new ExecutionContext(state.context(), state.executionContext(), newEnv, state.inStrictMode());
    ExecutionState newState(&state, newEc);
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

NEVER_INLINE Value ByteCodeInterpreter::callFunctionInWithScope(ExecutionState& state, CallFunctionInWithScope* code, ExecutionContext* ec, LexicalEnvironment* env, Value* argv)
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
    return FunctionObject::call(state, callee, receiverObj, code->m_argumentCount, argv);
}

NEVER_INLINE void ByteCodeInterpreter::declareFunctionDeclarations(ExecutionState& state, DeclareFunctionDeclarations* code, LexicalEnvironment* lexicalEnvironment, Value* stackStorage)
{
    InterpretedCodeBlock* cb = code->m_codeBlock;
    const CodeBlockVector& v = cb->childBlocks();
    size_t l = v.size();
    if (LIKELY(cb->canUseIndexedVariableStorage())) {
        for (size_t i = 0; i < l; i++) {
            if (v[i]->isFunctionDeclaration()) {
                AtomicString name = v[i]->functionName();
                FunctionObject* fn = new FunctionObject(state, v[i], lexicalEnvironment);
                const CodeBlock::IdentifierInfo& info = cb->identifierInfos()[cb->findName(name)];
                if (info.m_needToAllocateOnStack) {
                    stackStorage[info.m_indexForIndexedStorage] = fn;
                } else {
                    FunctionEnvironmentRecord* record = lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap() || record->isFunctionEnvironmentRecordNotIndexed());
                    ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[info.m_indexForIndexedStorage] = fn;
                }
            }
        }
    } else {
        for (size_t i = 0; i < l; i++) {
            if (v[i]->isFunctionDeclaration()) {
                AtomicString name = v[i]->functionName();
                FunctionObject* fn = new FunctionObject(state, v[i], lexicalEnvironment);
                LexicalEnvironment* env = lexicalEnvironment;

                while (!env->record()->isEvalTarget()) {
                    env = env->outerEnvironment();
                }

                while (env) {
                    auto record = env->record();
                    if (record->isEvalTarget()) {
                        auto result = env->record()->hasBinding(state, name);
                        if (result.m_index != SIZE_MAX) {
                            env->record()->initializeBinding(state, name, fn);
                            break;
                        }
                    }
                    env = env->outerEnvironment();
                }
                ASSERT(env);
            }
        }
    }
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectGetter(ExecutionState& state, ObjectDefineGetter* code, Value* registerFile)
{
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    String* pName = registerFile[code->m_objectPropertyNameRegisterIndex].toString(state);
    StringBuilder builder;
    builder.appendString("get ");
    builder.appendString(pName);
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(builder.finalize()));
    JSGetterSetter gs(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
    ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
    registerFile[code->m_objectRegisterIndex].toObject(state)->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, pName), desc);
}

NEVER_INLINE void ByteCodeInterpreter::defineObjectSetter(ExecutionState& state, ObjectDefineSetter* code, Value* registerFile)
{
    FunctionObject* fn = registerFile[code->m_objectPropertyValueRegisterIndex].asFunction();
    String* pName = registerFile[code->m_objectPropertyNameRegisterIndex].toString(state);
    StringBuilder builder;
    builder.appendString("set ");
    builder.appendString(pName);
    fn->defineOwnProperty(state, state.context()->staticStrings().name, ObjectPropertyDescriptor(builder.finalize()));
    JSGetterSetter gs(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
    ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
    registerFile[code->m_objectRegisterIndex].toObject(state)->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, pName), desc);
}

NEVER_INLINE void ByteCodeInterpreter::processException(ExecutionState& state, const Value& value, ExecutionContext* ecInput, size_t programCounter)
{
    ASSERT(state.context()->m_sandBoxStack.size());
    SandBox* sb = state.context()->m_sandBoxStack.back();

    LexicalEnvironment* env = ecInput->lexicalEnvironment();
    ExecutionContext* ec = ecInput;

    while (true) {
        if (env->record()->isGlobalEnvironmentRecord()) {
            break;
        } else if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            break;
        }
        env = env->outerEnvironment();
        ec = ecInput->parent();
    }

    bool alreadyExists = false;

    for (size_t i = 0; i < sb->m_stackTraceData.size(); i++) {
        if (sb->m_stackTraceData[i].first == ec) {
            alreadyExists = true;
            break;
        }
    }

    if (!alreadyExists) {
        if (env->record()->isGlobalEnvironmentRecord()) {
            CodeBlock* cb = env->record()->asGlobalEnvironmentRecord()->globalCodeBlock();
            ByteCodeBlock* b = cb->asInterpretedCodeBlock()->byteCodeBlock();
            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            if (programCounter != SIZE_MAX) {
                loc.byteCodePosition = programCounter - (size_t)b->m_code.data();
                loc.actualCodeBlock = b;
            }
            SandBox::StackTraceData data;
            data.loc = loc;
            data.fileName = cb->asInterpretedCodeBlock()->script()->fileName();
            data.source = cb->asInterpretedCodeBlock()->script()->src();
            sb->m_stackTraceData.pushBack(std::make_pair(ec, data));
        } else {
            FunctionObject* fn = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
            CodeBlock* cb = fn->codeBlock();
            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            if (cb->isInterpretedCodeBlock()) {
                ByteCodeBlock* b = cb->asInterpretedCodeBlock()->byteCodeBlock();
                if (programCounter != SIZE_MAX) {
                    loc.byteCodePosition = programCounter - (size_t)b->m_code.data();
                    loc.actualCodeBlock = b;
                }
            }
            SandBox::StackTraceData data;
            data.loc = loc;
            if (cb->isInterpretedCodeBlock() && cb->asInterpretedCodeBlock()->script()) {
                data.fileName = cb->asInterpretedCodeBlock()->script()->fileName();
            } else {
                StringBuilder builder;
                builder.appendString("function ");
                builder.appendString(cb->functionName().string());
                builder.appendString("() { ");
                builder.appendString("[native function]");
                builder.appendString(" } ");
                data.fileName = builder.finalize();
            }
            data.source = String::emptyString;
            sb->m_stackTraceData.pushBack(std::make_pair(ec, data));
        }
    }
    sb->throwException(state, value);
}
}
