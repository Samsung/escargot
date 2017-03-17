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

namespace Escargot {

#define ADD_PROGRAM_COUNTER(CodeType) programCounter += sizeof(CodeType);

ALWAYS_INLINE size_t jumpTo(char* codeBuffer, const size_t& jumpPosition)
{
    return (size_t)&codeBuffer[jumpPosition];
}

ALWAYS_INLINE size_t resolveProgramCounter(char* codeBuffer, const size_t programCounter)
{
    return programCounter - (size_t)codeBuffer;
}

Value ByteCodeInterpreter::interpret(ExecutionState& state, ByteCodeBlock* byteCodeBlock, register size_t programCounter, Value* registerFile, void* initAddressFiller)
{
    *((size_t*)initAddressFiller) = ((size_t) && FillOpcodeTableOpcodeLbl);
    {
        ExecutionContext* ec = state.executionContext();
        GlobalObject* globalObject = state.context()->globalObject();
        char* codeBuffer = byteCodeBlock->m_code.data();
        programCounter = (size_t)(&codeBuffer[programCounter]);

        try {
#define NEXT_INSTRUCTION() goto NextInstruction;

        NextInstruction:
            goto*(((ByteCode*)programCounter)->m_opcodeInAddress);

        LoadLiteralOpcodeLbl : {
            LoadLiteral* code = (LoadLiteral*)programCounter;
            registerFile[code->m_registerIndex] = code->m_value;
            ADD_PROGRAM_COUNTER(LoadLiteral);
            NEXT_INSTRUCTION();
        }

        MoveOpcodeLbl : {
            Move* code = (Move*)programCounter;
            ASSERT(code->m_registerIndex1 < (byteCodeBlock->m_requiredRegisterFileSizeInValueSize + byteCodeBlock->m_codeBlock->identifierOnStackCount() + 1));
            registerFile[code->m_registerIndex1] = registerFile[code->m_registerIndex0];
            ADD_PROGRAM_COUNTER(Move);
            NEXT_INSTRUCTION();
        }

        GetGlobalObjectOpcodeLbl : {
            GetGlobalObject* code = (GetGlobalObject*)programCounter;
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

        SetGlobalObjectOpcodeLbl : {
            SetGlobalObject* code = (SetGlobalObject*)programCounter;
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

        BinaryPlusOpcodeLbl : {
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

        BinaryMinusOpcodeLbl : {
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

        BinaryMultiplyOpcodeLbl : {
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
                ret = Value(Value::EncodeAsDouble, left.toNumber(state) * right.toNumber(state));
            }
            registerFile[code->m_dstIndex] = ret;
            ADD_PROGRAM_COUNTER(BinaryMultiply);
            NEXT_INSTRUCTION();
        }

        BinaryDivisionOpcodeLbl : {
            BinaryDivision* code = (BinaryDivision*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toNumber(state) / right.toNumber(state));
            ADD_PROGRAM_COUNTER(BinaryDivision);
            NEXT_INSTRUCTION();
        }

        BinaryEqualOpcodeLbl : {
            BinaryEqual* code = (BinaryEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryEqual);
            NEXT_INSTRUCTION();
        }

        BinaryNotEqualOpcodeLbl : {
            BinaryNotEqual* code = (BinaryNotEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotEqual);
            NEXT_INSTRUCTION();
        }

        BinaryStrictEqualOpcodeLbl : {
            BinaryStrictEqual* code = (BinaryStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryStrictEqual);
            NEXT_INSTRUCTION();
        }

        BinaryNotStrictEqualOpcodeLbl : {
            BinaryNotStrictEqual* code = (BinaryNotStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(!left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotStrictEqual);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOpcodeLbl : {
            BinaryLessThan* code = (BinaryLessThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThan);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOrEqualOpcodeLbl : {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThanOrEqual);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOpcodeLbl : {
            BinaryGreaterThan* code = (BinaryGreaterThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparison(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThan);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOrEqualOpcodeLbl : {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(abstractRelationalComparisonOrEqual(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThanOrEqual);
            NEXT_INSTRUCTION();
        }

        IncrementOpcodeLbl : {
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

        DecrementOpcodeLbl : {
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

        UnaryMinusOpcodeLbl : {
            UnaryMinus* code = (UnaryMinus*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(-val.toNumber(state));
            ADD_PROGRAM_COUNTER(UnaryMinus);
            NEXT_INSTRUCTION();
        }

        UnaryNotOpcodeLbl : {
            UnaryNot* code = (UnaryNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(!val.toBoolean(state));
            ADD_PROGRAM_COUNTER(UnaryNot);
            NEXT_INSTRUCTION();
        }

        GetObjectOpcodeLbl : {
            GetObject* code = (GetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            PointerValue* v;
            if (LIKELY(willBeObject.isObject() && (v = willBeObject.asPointerValue())->hasTag(g_arrayObjectTag))) {
                ArrayObject* arr = (ArrayObject*)v;
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx;
                    if (LIKELY(property.isUInt32()))
                        idx = property.asUInt32();
                    else {
                        idx = property.toString(state)->tryToUseAsArrayIndex();
                    }
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        if (LIKELY(idx < arr->getArrayLength(state))) {
                            const Value& v = arr->m_fastModeData[idx];
                            if (LIKELY(!v.isEmpty())) {
                                registerFile[code->m_storeRegisterIndex] = v;
                                ADD_PROGRAM_COUNTER(GetObject);
                                NEXT_INSTRUCTION();
                            }
                        }
                    }
                }
            }
            goto GetObjectOpcodeSlowCase;
        }

        SetObjectOpcodeLbl : {
            SetObject* code = (SetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            if (LIKELY(willBeObject.isObject() && (willBeObject.asPointerValue())->hasTag(g_arrayObjectTag))) {
                ArrayObject* arr = willBeObject.asObject()->asArrayObject();
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx;
                    if (LIKELY(property.isUInt32()))
                        idx = property.asUInt32();
                    else {
                        idx = property.toString(state)->tryToUseAsArrayIndex();
                    }
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        uint32_t len = arr->getArrayLength(state);
                        if (UNLIKELY(len <= idx)) {
                            if (UNLIKELY(!arr->isExtensible())) {
                                goto SetObjectOpcodeSlowCase;
                            }
                            if (UNLIKELY(!arr->setArrayLength(state, idx + 1)) || UNLIKELY(!arr->isFastModeArray())) {
                                goto SetObjectOpcodeSlowCase;
                            }
                        }
                        arr->m_fastModeData[idx] = registerFile[code->m_loadRegisterIndex];
                        ADD_PROGRAM_COUNTER(SetObject);
                        NEXT_INSTRUCTION();
                    }
                }
            }
            goto SetObjectOpcodeSlowCase;
        }

        GetObjectPreComputedCaseOpcodeLbl : {
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

        SetObjectPreComputedCaseOpcodeLbl : {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            setObjectPreComputedCaseOperation(state, willBeObject.toObject(state), code->m_propertyName, registerFile[code->m_loadRegisterIndex], *code->m_inlineCache, byteCodeBlock);
            ADD_PROGRAM_COUNTER(SetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        JumpOpcodeLbl : {
            Jump* code = (Jump*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            programCounter = code->m_jumpPosition;
            NEXT_INSTRUCTION();
        }

        JumpIfTrueOpcodeLbl : {
            JumpIfTrue* code = (JumpIfTrue*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfTrue);
            }
            NEXT_INSTRUCTION();
        }

        JumpIfFalseOpcodeLbl : {
            JumpIfFalse* code = (JumpIfFalse*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (!registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = code->m_jumpPosition;
            } else {
                ADD_PROGRAM_COUNTER(JumpIfFalse);
            }
            NEXT_INSTRUCTION();
        }

        CallFunctionOpcodeLbl : {
            CallFunction* code = (CallFunction*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            registerFile[code->m_resultIndex] = FunctionObject::call(state, callee, Value(), code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(CallFunction);
            NEXT_INSTRUCTION();
        }

        CallFunctionWithReceiverOpcodeLbl : {
            CallFunctionWithReceiver* code = (CallFunctionWithReceiver*)programCounter;
            const Value& callee = registerFile[code->m_calleeIndex];
            const Value& receiver = registerFile[code->m_receiverIndex];
            registerFile[code->m_resultIndex] = FunctionObject::call(state, callee, receiver, code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(CallFunctionWithReceiver);
            NEXT_INSTRUCTION();
        }

        LoadByHeapIndexOpcodeLbl : {
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

        StoreByHeapIndexOpcodeLbl : {
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

        BinaryModOpcodeLbl : {
            BinaryMod* code = (BinaryMod*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = modOperation(state, left, right);
            ADD_PROGRAM_COUNTER(BinaryMod);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseAndOpcodeLbl : {
            BinaryBitwiseAnd* code = (BinaryBitwiseAnd*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) & right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseAnd);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseOrOpcodeLbl : {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) | right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseOr);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseXorOpcodeLbl : {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_dstIndex] = Value(left.toInt32(state) ^ right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseXor);
            NEXT_INSTRUCTION();
        }

        BinaryLeftShiftOpcodeLbl : {
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

        BinarySignedRightShiftOpcodeLbl : {
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

        BinaryUnsignedRightShiftOpcodeLbl : {
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

        UnaryBitwiseNotOpcodeLbl : {
            UnaryBitwiseNot* code = (UnaryBitwiseNot*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(~val.toInt32(state));
            ADD_PROGRAM_COUNTER(UnaryBitwiseNot);
            NEXT_INSTRUCTION();
        }

        ReturnFunctionWithValueOpcodeLbl : {
            ReturnFunctionWithValue* code = (ReturnFunctionWithValue*)programCounter;
            return registerFile[code->m_registerIndex];
        }

        ReturnFunctionOpcodeLbl : {
            return Value();
        }

        ToNumberOpcodeLbl : {
            ToNumber* code = (ToNumber*)programCounter;
            const Value& val = registerFile[code->m_srcIndex];
            registerFile[code->m_dstIndex] = Value(val.toNumber(state));
            ADD_PROGRAM_COUNTER(ToNumber);
            NEXT_INSTRUCTION();
        }

        CreateObjectOpcodeLbl : {
            CreateObject* code = (CreateObject*)programCounter;
            registerFile[code->m_registerIndex] = new Object(state);
            ADD_PROGRAM_COUNTER(CreateObject);
            NEXT_INSTRUCTION();
        }

        CreateArrayOpcodeLbl : {
            CreateArray* code = (CreateArray*)programCounter;
            ArrayObject* arr = new ArrayObject(state);
            arr->setArrayLength(state, code->m_length);
            registerFile[code->m_registerIndex] = arr;
            ADD_PROGRAM_COUNTER(CreateArray);
            NEXT_INSTRUCTION();
        }

        ObjectDefineOwnPropertyOperationOpcodeLbl : {
            ObjectDefineOwnPropertyOperation* code = (ObjectDefineOwnPropertyOperation*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            ObjectPropertyName objPropName = ObjectPropertyName(state, property);
            // http://www.ecma-international.org/ecma-262/6.0/#sec-__proto__-property-names-in-object-initializers
            if (property.toString(state)->equals("__proto__")) {
                willBeObject.asObject()->setPrototype(state, registerFile[code->m_loadRegisterIndex]);
            } else {
                willBeObject.asObject()->defineOwnProperty(state, objPropName, ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], ObjectPropertyDescriptor::AllPresent));
            }
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        ObjectDefineOwnPropertyWithNameOperationOpcodeLbl : {
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

        ArrayDefineOwnPropertyOperationOpcodeLbl : {
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

        NewOperationOpcodeLbl : {
            NewOperation* code = (NewOperation*)programCounter;
            registerFile[code->m_resultIndex] = newOperation(state, registerFile[code->m_calleeIndex], code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(NewOperation);
            NEXT_INSTRUCTION();
        }

        UnaryTypeofOpcodeLbl : {
            UnaryTypeof* code = (UnaryTypeof*)programCounter;
            Value val;
            if (code->m_id.string()->length()) {
                val = loadByName(state, ec->lexicalEnvironment(), code->m_id, false);
            } else {
                val = registerFile[code->m_srcIndex];
            }
            if (val.isUndefined())
                val = state.context()->staticStrings().undefined.string();
            else if (val.isNull())
                val = state.context()->staticStrings().object.string();
            else if (val.isBoolean())
                val = state.context()->staticStrings().boolean.string();
            else if (val.isNumber())
                val = state.context()->staticStrings().number.string();
            else if (val.isString())
                val = state.context()->staticStrings().string.string();
            else {
                ASSERT(val.isPointerValue());
                PointerValue* p = val.asPointerValue();
                if (p->isFunctionObject()) {
                    val = state.context()->staticStrings().function.string();
                } else {
                    val = state.context()->staticStrings().object.string();
                }
            }

            registerFile[code->m_dstIndex] = val;
            ADD_PROGRAM_COUNTER(UnaryTypeof);
            NEXT_INSTRUCTION();
        }

        GetObjectOpcodeSlowCase : {
            GetObject* code = (GetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            Object* obj;
            if (LIKELY(willBeObject.isObject())) {
                obj = willBeObject.asObject();
            } else {
                obj = fastToObject(state, willBeObject);
            }
            registerFile[code->m_storeRegisterIndex] = obj->getIndexedProperty(state, property).value(state, obj);
            ADD_PROGRAM_COUNTER(GetObject);
            NEXT_INSTRUCTION();
        }

        SetObjectOpcodeSlowCase : {
            SetObject* code = (SetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            Object* obj = willBeObject.toObject(state);
            bool result = obj->setIndexedProperty(state, property, registerFile[code->m_loadRegisterIndex]);
            if (UNLIKELY(!result)) {
                if (state.inStrictMode()) {
                    // copy assignee for generating right execution result
                    registerFile[code->m_objectRegisterIndex] = registerFile[code->m_loadRegisterIndex];
                    Object::throwCannotWriteError(state, PropertyName(state, property.toString(state)));
                }
            }
            ADD_PROGRAM_COUNTER(SetObject);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionDeclarationOpcodeLbl : {
            DeclareFunctionDeclaration* code = (DeclareFunctionDeclaration*)programCounter;
            registerFile[1] = new FunctionObject(state, code->m_codeBlock, ec->lexicalEnvironment());
            ADD_PROGRAM_COUNTER(DeclareFunctionDeclaration);
            NEXT_INSTRUCTION();
        }

        LoadByNameOpcodeLbl : {
            LoadByName* code = (LoadByName*)programCounter;
            registerFile[code->m_registerIndex] = loadByName(state, ec->lexicalEnvironment(), code->m_name);
            ADD_PROGRAM_COUNTER(LoadByName);
            NEXT_INSTRUCTION();
        }

        StoreByNameOpcodeLbl : {
            StoreByName* code = (StoreByName*)programCounter;
            storeByName(state, ec->lexicalEnvironment(), code->m_name, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByName);
            NEXT_INSTRUCTION();
        }

        CallEvalFunctionOpcodeLbl : {
            CallEvalFunction* code = (CallEvalFunction*)programCounter;
            Value eval = loadByName(state, ec->lexicalEnvironment(), state.context()->staticStrings().eval);
            if (eval.equalsTo(state, state.context()->globalObject()->eval())) {
                // do eval
                Value arg;
                if (code->m_argumentCount) {
                    arg = registerFile[code->m_argumentsStartIndex];
                }
                Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
                registerFile[code->m_resultIndex] = state.context()->globalObject()->evalLocal(state, arg, stackStorage[0], byteCodeBlock->m_codeBlock);
            } else {
                registerFile[code->m_resultIndex] = FunctionObject::call(state, eval, Value(), code->m_argumentCount, &registerFile[code->m_argumentsStartIndex]);
            }
            ADD_PROGRAM_COUNTER(CallEvalFunction);
            NEXT_INSTRUCTION();
        }

        TryOperationOpcodeLbl : {
            TryOperation* code = (TryOperation*)programCounter;
            programCounter = tryOperation(state, code, ec, ec->lexicalEnvironment(), programCounter, byteCodeBlock, registerFile);
            NEXT_INSTRUCTION();
        }

        TryCatchWithBodyEndOpcodeLbl : {
            (*(state.rareData()->m_controlFlowRecord))[state.rareData()->m_controlFlowRecord->size() - 1] = nullptr;
            return Value();
        }

        FinallyEndOpcodeLbl : {
            FinallyEnd* code = (FinallyEnd*)programCounter;
            ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
            state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);
            if (record) {
                if (record->reason() == ControlFlowRecord::NeedsJump) {
                    size_t pos = record->wordValue();
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                        return Value();
                    } else
                        programCounter = jumpTo(codeBuffer, pos);
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

        ThrowOperationOpcodeLbl : {
            ThrowOperation* code = (ThrowOperation*)programCounter;
            state.context()->throwException(state, registerFile[code->m_registerIndex]);
        }

        WithOperationOpcodeLbl : {
            WithOperation* code = (WithOperation*)programCounter;
            size_t newPc = programCounter;
            Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
            Value v = withOperation(state, code, registerFile[code->m_registerIndex].toObject(state), ec, ec->lexicalEnvironment(), newPc, byteCodeBlock, registerFile, stackStorage);
            if (!v.isEmpty()) {
                return v;
            }
            programCounter = newPc;
            NEXT_INSTRUCTION();
        }

        JumpComplexCaseOpcodeLbl : {
            JumpComplexCase* code = (JumpComplexCase*)programCounter;
            state.ensureRareData()->m_controlFlowRecord->back() = code->m_controlFlowRecord->clone();
            return Value();
        }

        EnumerateObjectOpcodeLbl : {
            EnumerateObject* code = (EnumerateObject*)programCounter;
            auto data = executeEnumerateObject(state, registerFile[code->m_objectRegisterIndex].toObject(state));
            registerFile[code->m_dataRegisterIndex] = Value((PointerValue*)data);
            ADD_PROGRAM_COUNTER(EnumerateObject);
            NEXT_INSTRUCTION();
        }

        CheckIfKeyIsLastOpcodeLbl : {
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

            if (data->m_object->isArrayObject() && data->m_object->length(state) != data->m_originalLength) {
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

        EnumerateObjectKeyOpcodeLbl : {
            EnumerateObjectKey* code = (EnumerateObjectKey*)programCounter;
            EnumerateObjectData* data = (EnumerateObjectData*)registerFile[code->m_dataRegisterIndex].asPointerValue();
            data->m_idx++;
            registerFile[code->m_registerIndex] = Value(data->m_keys[data->m_idx - 1]).toString(state);
            ADD_PROGRAM_COUNTER(EnumerateObjectKey);
            NEXT_INSTRUCTION();
        }

        LoadRegexpOpcodeLbl : {
            LoadRegexp* code = (LoadRegexp*)programCounter;
            auto reg = new RegExpObject(state, code->m_body, code->m_option);
            registerFile[code->m_registerIndex] = reg;
            ADD_PROGRAM_COUNTER(LoadRegexp);
            NEXT_INSTRUCTION();
        }

        UnaryDeleteOpcodeLbl : {
            UnaryDelete* code = (UnaryDelete*)programCounter;
            deleteOperation(state, ec->lexicalEnvironment(), code, registerFile);
            ADD_PROGRAM_COUNTER(UnaryDelete);
            NEXT_INSTRUCTION();
        }

        BinaryInOperationOpcodeLbl : {
            BinaryInOperation* code = (BinaryInOperation*)programCounter;
            if (!registerFile[code->m_srcIndex1].isObject())
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "type of rvalue is not Object");
            auto result = registerFile[code->m_srcIndex1].toObject(state)->get(state, ObjectPropertyName(state, registerFile[code->m_srcIndex0]));
            registerFile[code->m_dstIndex] = Value(result.hasValue());
            ADD_PROGRAM_COUNTER(BinaryInOperation);
            NEXT_INSTRUCTION();
        }

        BinaryInstanceOfOperationOpcodeLbl : {
            BinaryInstanceOfOperation* code = (BinaryInstanceOfOperation*)programCounter;
            registerFile[code->m_dstIndex] = instanceOfOperation(state, registerFile[code->m_srcIndex0], registerFile[code->m_srcIndex1]);
            ADD_PROGRAM_COUNTER(BinaryInstanceOfOperation);
            NEXT_INSTRUCTION();
        }

        ObjectDefineGetterOpcodeLbl : {
            ObjectDefineGetter* code = (ObjectDefineGetter*)programCounter;
            JSGetterSetter gs(registerFile[code->m_objectPropertyValueRegisterIndex].asFunction(), Value(Value::EmptyValue));
            ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
            registerFile[code->m_objectRegisterIndex].toObject(state)->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, registerFile[code->m_objectPropertyNameRegisterIndex]), desc);
            ADD_PROGRAM_COUNTER(ObjectDefineGetter);
            NEXT_INSTRUCTION();
        }

        ObjectDefineSetterOpcodeLbl : {
            ObjectDefineSetter* code = (ObjectDefineSetter*)programCounter;
            JSGetterSetter gs(Value(Value::EmptyValue), registerFile[code->m_objectPropertyValueRegisterIndex].asFunction());
            ObjectPropertyDescriptor desc(gs, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent));
            registerFile[code->m_objectRegisterIndex].toObject(state)->defineOwnPropertyThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, registerFile[code->m_objectPropertyNameRegisterIndex]), desc);
            ADD_PROGRAM_COUNTER(ObjectDefineGetter);
            NEXT_INSTRUCTION();
        }

        CallFunctionInWithScopeOpcodeLbl : {
            CallFunctionInWithScope* code = (CallFunctionInWithScope*)programCounter;
            registerFile[code->m_resultIndex] = callFunctionInWithScope(state, code, ec, ec->lexicalEnvironment(), &registerFile[code->m_argumentsStartIndex]);
            ADD_PROGRAM_COUNTER(CallFunctionInWithScope);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionExpressionOpcodeLbl : {
            DeclareFunctionExpression* code = (DeclareFunctionExpression*)programCounter;
            registerFile[code->m_registerIndex] = new FunctionObject(state, code->m_codeBlock, ec->lexicalEnvironment());
            ADD_PROGRAM_COUNTER(DeclareFunctionExpression);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionDeclarationsInGlobalOpcodeLbl : {
            DeclareFunctionDeclarationsInGlobal* code = (DeclareFunctionDeclarationsInGlobal*)programCounter;
            declareFunctionDeclarationsInGlobal(state, code, globalObject, ec->lexicalEnvironment());
            ADD_PROGRAM_COUNTER(DeclareFunctionDeclarationsInGlobal);
            NEXT_INSTRUCTION();
        }

        ReturnFunctionSlowCaseOpcodeLbl : {
            ReturnFunctionSlowCase* code = (ReturnFunctionSlowCase*)programCounter;
            Value ret;
            if (code->m_registerIndex != std::numeric_limits<ByteCodeRegisterIndex>::max()) {
                ret = registerFile[code->m_registerIndex];
            }
            if (UNLIKELY(state.rareData() != nullptr)) {
                if (state.rareData()->m_controlFlowRecord && state.rareData()->m_controlFlowRecord->size()) {
                    state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, ret, state.rareData()->m_controlFlowRecord->size());
                }
            }
            return ret;
        }

        ThrowStaticErrorOperationOpcodeLbl : {
            ThrowStaticErrorOperation* code = (ThrowStaticErrorOperation*)programCounter;
            ErrorObject::throwBuiltinError(state, (ErrorObject::Code)code->m_errorKind, code->m_errorMessage);
        }

        EndOpcodeLbl:
        CallNativeFunctionOpcodeLbl:
        CallBoundFunctionOpcodeLbl:
            return registerFile[0];

        } catch (const Value& v) {
            processException(state, v, ec, programCounter);
        }
    }
FillOpcodeTableOpcodeLbl : {
#define REGISTER_TABLE(opcode, pushCount, popCount) g_opcodeTable.m_table[opcode##Opcode] = &&opcode##OpcodeLbl;
    FOR_EACH_BYTECODE_OP(REGISTER_TABLE);
#undef REGISTER_TABLE
    return Value();
}
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
        if (std::isnan(lvalue) || std::isnan(rvalue))
            ret = Value(std::numeric_limits<double>::quiet_NaN());
        else if (std::isinf(lvalue) || rvalue == 0 || rvalue == -0.0)
            ret = Value(std::numeric_limits<double>::quiet_NaN());
        else if (std::isinf(rvalue))
            ret = Value(lvalue);
        else if (lvalue == 0.0) {
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
    if (!callee.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
    }
    FunctionObject* function = callee.asFunction();
    if (!function->isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, function->codeBlock()->functionName().string(), false, String::emptyString, errorMessage_New_NotConstructor);
    }
    Object* receiver;
    CodeBlock* cb = function->codeBlock();
    if (cb->hasCallNativeFunctionCode()) {
        receiver = cb->nativeFunctionConstructor()(state, argc, argv);
    } else {
        receiver = new Object(state);
    }

    if (function->getFunctionPrototype(state).isObject())
        receiver->setPrototype(state, function->getFunctionPrototype(state));
    else
        receiver->setPrototype(state, new Object(state));

    Value res = function->call(state, receiver, argc, argv, true);
    if (res.isObject())
        return res.asObject();
    else
        return receiver;
}

NEVER_INLINE Value ByteCodeInterpreter::instanceOfOperation(ExecutionState& state, const Value& left, const Value& right)
{
    if (!right.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    if (left.isObject()) {
        FunctionObject* C = right.asFunction();
        Value P = C->getFunctionPrototype(state);
        Value O = left.asObject()->getPrototype(state);
        if (P.isObject()) {
            while (!O.isUndefinedOrNull()) {
                if (P == O) {
                    return Value(true);
                }
                O = O.asObject()->getPrototype(state);
            }
        } else {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_InvalidPrototypeProperty);
        }
    }
    return Value(false);
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
        bool result = obj->deleteOwnProperty(state, ObjectPropertyName(state, p));
        if (!result && state.inStrictMode())
            Object::throwCannotDeleteError(state, ObjectPropertyName(state, p).toPropertyName(state));
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
            Object* protoObject = obj->getPrototypeObject();
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

    Object* orgObj = obj;
    // inlineCache.m_cache.insert(0, GetObjectInlineCacheData());
    inlineCache.m_cache.insert(inlineCache.m_cache.begin(), GetObjectInlineCacheData());

    ObjectStructureChain* cachedHiddenClassChain = &inlineCache.m_cache[0].m_cachedhiddenClassChain;

    ObjectStructureChainItem newItem;
    while (true) {
        newItem.m_objectStructure = obj->structure();

        cachedHiddenClassChain->push_back(newItem);
        size_t idx = obj->structure()->findProperty(state, name);
        if (obj->structure()->isStructureWithFastAccess()) {
            block->m_objectStructuresInUse->insert(obj->structure());
        }
        if (idx != SIZE_MAX) {
            inlineCache.m_cache[0].m_cachedIndex = idx;
            break;
        }
        obj = obj->getPrototypeObject();
        if (!obj) {
            break;
        }
    }

    if (inlineCache.m_cache[0].m_cachedIndex != SIZE_MAX) {
        return obj->getOwnPropertyUtilForObject(state, inlineCache.m_cache[0].m_cachedIndex, orgObj);
    } else {
        return Value();
    }
}

ALWAYS_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperation(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    Object* originalObject = obj;

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
                Object* o = obj->getPrototypeObject();
                if (UNLIKELY(!o)) {
                    miss = true;
                    break;
                }
                obj = o;
            }
        }
        if (LIKELY(!miss)) {
            if (inlineCache.m_cachedhiddenClassChain[cSiz - 1].m_objectStructure == obj->structure()) {
                // cache hit!
                obj = originalObject;
                ASSERT(!obj->structure()->isStructureWithFastAccess());
                obj->m_values.push_back(value, inlineCache.m_hiddenClassWillBe->propertyCount());
                obj->m_structure = inlineCache.m_hiddenClassWillBe;
                return;
            }
        }
    }

    setObjectPreComputedCaseOperationCacheMiss(state, originalObject, name, value, inlineCache, block);
}

NEVER_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* originalObject, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache, ByteCodeBlock* block)
{
    // cache miss
    if (inlineCache.m_cacheMissCount > 16) {
        originalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, originalObject);
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

        obj->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value);
        auto desc = obj->structure()->readProperty(state, idx).m_descriptor;
        if (desc.isPlainDataProperty() && desc.isWritable()) {
            inlineCache.m_cachedIndex = idx;
            inlineCache.m_cachedhiddenClassChain.push_back(newItem);
        }
    } else {
        Object* orgObject = obj;
        if (UNLIKELY(obj->structure()->isStructureWithFastAccess())) {
            inlineCache.invalidateCache();
            orgObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, orgObject);
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
        bool s = orgObject->set(state, ObjectPropertyName(state, name), value, orgObject);
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

        auto result = orgObject->get(state, ObjectPropertyName(state, name), orgObject);
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
                    String* key = name.toValue(state).toString(state);
                    auto iter = eData->keyStringSet->find(key);
                    if (iter == eData->keyStringSet->end()) {
                        eData->keyStringSet->insert(key);
                        eData->data->m_keys.pushBack(name.toValue(state));
                    }
                } else if (self == eData->obj) {
                    // 12.6.4 The values of [[Enumerable]] attributes are not considered
                    // when determining if a property of a prototype object is shadowed by a previous object on the prototype chain.
                    String* key = name.toValue(state).toString(state);
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
                eData->data->m_keys[(*eData->idx)++] = name.toValue(state);
            }
            return true;
        },
                                       &eData);
        ASSERT(ownKeyCount == idx);
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
        if (std::find(oldKeys.begin(), oldKeys.begin() + data->m_idx, key) == oldKeys.begin() + data->m_idx) {
            // If a property that has not yet been visited during enumeration is deleted, then it will not be visited.
            if (std::find(oldKeys.begin() + data->m_idx, oldKeys.end(), key) != oldKeys.end()) {
                // If new properties are added to the object being enumerated during enumeration,
                // the newly added properties are not guaranteed to be visited in the active enumeration.
                differenceKeys.push_back(key);
            }
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
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.string(), false, String::emptyString, errorMessage_IsNotDefined);
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

NEVER_INLINE void ByteCodeInterpreter::setGlobalObjectSlowCase(ExecutionState& state, Object* go, SetGlobalObject* code, const Value& value, ByteCodeBlock* block)
{
    size_t idx = go->structure()->findProperty(state, code->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        if (UNLIKELY(state.inStrictMode())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.string(), false, String::emptyString, errorMessage_IsNotDefined);
        }
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

        go->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value);
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
        size_t unused;
        interpret(state, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile, &unused);
        programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
    } catch (const Value& val) {
        state.context()->m_sandBoxStack.back()->m_stackTraceData.clear();
        if (code->m_hasCatch == false) {
            state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
            programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
        } else {
            // setup new env
            EnvironmentRecord* newRecord = new DeclarativeEnvironmentRecordNotIndexed();
            newRecord->createMutableBinding(state, code->m_catchVariableName);
            newRecord->setMutableBinding(state, code->m_catchVariableName, val);
            LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
            ExecutionContext* newEc = new ExecutionContext(state.context(), state.executionContext(), newEnv, state.inStrictMode());

            try {
                ExecutionState newState(&state, newEc);
                newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;
                clearStack<386>();
                size_t unused;
                interpret(newState, byteCodeBlock, code->m_catchPosition, registerFile, &unused);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            } catch (const Value& val) {
                state.context()->m_sandBoxStack.back()->m_stackTraceData.clear();
                // NOTE: consider throw within catch clause
                state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            }
        }
    }
    return programCounter;
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

    size_t unused;
    interpret(newState, byteCodeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile, &unused);

    ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
    state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);

    if (record) {
        if (record->reason() == ControlFlowRecord::NeedsJump) {
            size_t pos = record->wordValue();
            record->m_count--;
            if (record->count()) {
                state.rareData()->m_controlFlowRecord->back() = record;
            } else
                programCounter = jumpTo(codeBuffer, pos);
            return Value(Value::EmptyValue);
        } else {
            ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
            record->m_count--;
            if (record->count()) {
                state.rareData()->m_controlFlowRecord->back() = record;
                return Value();
            } else {
                return record->value();
            }
        }
    } else {
        programCounter = jumpTo(codeBuffer, code->m_withEndPostion);
    }
    return Value(Value::EmptyValue);
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

NEVER_INLINE void ByteCodeInterpreter::declareFunctionDeclarationsInGlobal(ExecutionState& state, DeclareFunctionDeclarationsInGlobal* code, GlobalObject* globalObject, LexicalEnvironment* lexicalEnvironment)
{
    const CodeBlockVector& v = code->m_codeBlock->childBlocks();
    size_t l = v.size();
    for (size_t i = 0; i < l; i++) {
        if (v[i]->isFunctionDeclaration())
            globalObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(v[i]->functionName()), new FunctionObject(state, v[i], lexicalEnvironment), globalObject);
    }
}

NEVER_INLINE void ByteCodeInterpreter::processException(ExecutionState& state, const Value& value, ExecutionContext* ec, size_t programCounter)
{
    ASSERT(state.context()->m_sandBoxStack.size());
    SandBox* sb = state.context()->m_sandBoxStack.back();
    if (ec->m_lexicalEnvironment->record()->isDeclarativeEnvironmentRecord()) {
        if (ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            FunctionObject* fn = ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
            CodeBlock* cb = fn->codeBlock();
            ByteCodeBlock* b = cb->byteCodeBlock();
            ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
            if (programCounter != SIZE_MAX) {
                loc = b->computeNodeLOCFromByteCode(state.context(), programCounter - (size_t)b->m_code.data(), cb);
            }
            if (sb->m_stackTraceData.size() == 0 || sb->m_stackTraceData.back().first != ec) {
                SandBox::StackTraceData data;
                data.loc = loc;
                if (cb->script())
                    data.fileName = cb->script()->fileName();
                else {
                    StringBuilder builder;
                    builder.appendString("function ");
                    builder.appendString(cb->functionName().string());
                    builder.appendString("() { ");
                    builder.appendString("[native function]");
                    builder.appendString(" } ");
                    data.fileName = builder.finalize();
                }
                sb->m_stackTraceData.pushBack(std::make_pair(ec, data));
            }
        }
    } else if (ec->m_lexicalEnvironment->record()->isGlobalEnvironmentRecord()) {
        CodeBlock* cb = ec->m_lexicalEnvironment->record()->asGlobalEnvironmentRecord()->globalCodeBlock();
        ByteCodeBlock* b = cb->byteCodeBlock();
        ExtendedNodeLOC loc(SIZE_MAX, SIZE_MAX, SIZE_MAX);
        if (programCounter != SIZE_MAX) {
            loc = b->computeNodeLOCFromByteCode(state.context(), programCounter - (size_t)b->m_code.data(), cb);
        }
        SandBox::StackTraceData data;
        data.loc = loc;
        data.fileName = cb->script()->fileName();
        sb->m_stackTraceData.pushBack(std::make_pair(ec, data));
    }
    sb->throwException(state, value);
}
}
