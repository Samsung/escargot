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
#include "../third_party/checked_arithmetic/CheckedArithmetic.h"

namespace Escargot {

size_t g_arrayObjectTag;
size_t g_stringTag;

NEVER_INLINE void registerOpcode(Opcode opcode, void* opcodeAddress)
{
    static std::unordered_set<void*> labelAddressChecker;
    if (labelAddressChecker.find(opcodeAddress) != labelAddressChecker.end()) {
        ESCARGOT_LOG_ERROR("%d\n", opcode);
        RELEASE_ASSERT_NOT_REACHED();
    }

    static int cnt = 0;
    g_opcodeTable.m_table[opcode] = opcodeAddress;
    g_opcodeTable.m_reverseTable[labelAddressChecker.size()] = std::make_pair(opcodeAddress, opcode);
    labelAddressChecker.insert(opcodeAddress);

    if (opcode == EndOpcode) {
        labelAddressChecker.clear();
    }
}

#define ADD_PROGRAM_COUNTER(CodeType) programCounter += sizeof(CodeType);

ALWAYS_INLINE size_t jumpTo(char* codeBuffer, const size_t& jumpPosition)
{
    return (size_t)&codeBuffer[jumpPosition];
}

ALWAYS_INLINE size_t resolveProgramCounter(char* codeBuffer, const size_t programCounter)
{
    return programCounter - (size_t)codeBuffer;
}

void ByteCodeInterpreter::interpret(ExecutionState& state, CodeBlock* codeBlock, register size_t programCounter, Value* registerFile, Value* stackStorage)
{
    if (UNLIKELY(codeBlock == nullptr)) {
        goto FillOpcodeTable;
    }
    {
        ASSERT(codeBlock->byteCodeBlock() != nullptr);
        ExecutionContext* ec = state.executionContext();
        LexicalEnvironment* env = ec->lexicalEnvironment();
        EnvironmentRecord* record = env->record();
        Value thisValue(Value::EmptyValue);
        GlobalObject* globalObject = state.context()->globalObject();
        char* codeBuffer = codeBlock->byteCodeBlock()->m_code.data();
        programCounter = (size_t)(&codeBuffer[programCounter]);

        goto*(((ByteCode*)programCounter)->m_opcodeInAddress);

        try {
#define NEXT_INSTRUCTION() goto*(((ByteCode*)programCounter)->m_opcodeInAddress);

        LoadLiteralOpcodeLbl : {
            LoadLiteral* code = (LoadLiteral*)programCounter;
            registerFile[code->m_registerIndex] = code->m_value;
            ADD_PROGRAM_COUNTER(LoadLiteral);
            NEXT_INSTRUCTION();
        }

        MoveOpcodeLbl : {
            Move* code = (Move*)programCounter;
            registerFile[code->m_registerIndex1] = registerFile[code->m_registerIndex0];
            ADD_PROGRAM_COUNTER(Move);
            NEXT_INSTRUCTION();
        }

        LoadByStackIndexOpcodeLbl : {
            LoadByStackIndex* code = (LoadByStackIndex*)programCounter;
            registerFile[code->m_registerIndex] = stackStorage[code->m_index];
            ADD_PROGRAM_COUNTER(LoadByStackIndex);
            NEXT_INSTRUCTION();
        }

        StoreByStackIndexOpcodeLbl : {
            StoreByStackIndex* code = (StoreByStackIndex*)programCounter;
            stackStorage[code->m_index] = registerFile[code->m_registerIndex];
            ADD_PROGRAM_COUNTER(StoreByStackIndex);
            NEXT_INSTRUCTION();
        }

        LoadByHeapIndexOpcodeLbl : {
            LoadByHeapIndex* code = (LoadByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = env;
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            registerFile[code->m_registerIndex] = ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index];
            ADD_PROGRAM_COUNTER(LoadByHeapIndex);
            NEXT_INSTRUCTION();
        }

        StoreByHeapIndexOpcodeLbl : {
            StoreByHeapIndex* code = (StoreByHeapIndex*)programCounter;
            LexicalEnvironment* upperEnv = env;
            for (size_t i = 0; i < code->m_upperIndex; i++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index] = registerFile[code->m_registerIndex];
            ADD_PROGRAM_COUNTER(StoreByHeapIndex);
            NEXT_INSTRUCTION();
        }

        GetGlobalObjectOpcodeLbl : {
            GetGlobalObject* code = (GetGlobalObject*)programCounter;
            if (LIKELY(globalObject->structure()->version() == code->m_savedGlobalObjectVersion)) {
                registerFile[code->m_registerIndex] = globalObject->uncheckedGetOwnDataProperty(state, code->m_cachedIndex);
            } else {
                registerFile[code->m_registerIndex] = getGlobalObjectSlowCase(state, globalObject, code);
            }
            ADD_PROGRAM_COUNTER(GetGlobalObject);
            NEXT_INSTRUCTION();
        }

        SetGlobalObjectOpcodeLbl : {
            SetGlobalObject* code = (SetGlobalObject*)programCounter;
            if (LIKELY(globalObject->structure()->version() == code->m_savedGlobalObjectVersion)) {
                globalObject->uncheckedSetOwnDataProperty(state, code->m_cachedIndex, registerFile[code->m_registerIndex]);
            } else {
                setGlobalObjectSlowCase(state, globalObject, code, registerFile[code->m_registerIndex]);
            }
            ADD_PROGRAM_COUNTER(SetGlobalObject);
            NEXT_INSTRUCTION();
        }

        GetThisOpcodeLbl : {
            GetThis* code = (GetThis*)programCounter;
            if (UNLIKELY(thisValue.isEmpty())) {
                thisValue = env->getThisBinding();
            }
            registerFile[code->m_registerIndex] = thisValue;
            ADD_PROGRAM_COUNTER(GetThis);
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
            registerFile[code->m_srcIndex0] = ret;
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
            registerFile[code->m_srcIndex0] = ret;
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
                if ((!a || !b) && (a >> 31 || b >> 31)) { // -1 * 0 should be treated as -0, not +0
                    ret = Value(left.toNumber(state) * right.toNumber(state));
                } else {
                    int32_t c = right.asInt32();
                    bool result = ArithmeticOperations<int32_t, int32_t, int32_t>::multiply(a, b, c);
                    if (LIKELY(result)) {
                        ret = Value(c);
                    } else {
                        ret = Value(Value::EncodeAsDouble, left.toNumber(state) * right.toNumber(state));
                    }
                }
            } else {
                ret = Value(Value::EncodeAsDouble, left.toNumber(state) * right.toNumber(state));
            }
            registerFile[code->m_srcIndex0] = ret;
            ADD_PROGRAM_COUNTER(BinaryMultiply);
            NEXT_INSTRUCTION();
        }

        BinaryDivisionOpcodeLbl : {
            BinaryDivision* code = (BinaryDivision*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toNumber(state) / right.toNumber(state));
            ADD_PROGRAM_COUNTER(BinaryDivision);
            NEXT_INSTRUCTION();
        }

        BinaryEqualOpcodeLbl : {
            BinaryEqual* code = (BinaryEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryEqual);
            NEXT_INSTRUCTION();
        }

        BinaryNotEqualOpcodeLbl : {
            BinaryNotEqual* code = (BinaryNotEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(!left.abstractEqualsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotEqual);
            NEXT_INSTRUCTION();
        }

        BinaryStrictEqualOpcodeLbl : {
            BinaryStrictEqual* code = (BinaryStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryStrictEqual);
            NEXT_INSTRUCTION();
        }

        BinaryNotStrictEqualOpcodeLbl : {
            BinaryNotStrictEqual* code = (BinaryNotStrictEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(!left.equalsTo(state, right));
            ADD_PROGRAM_COUNTER(BinaryNotStrictEqual);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOpcodeLbl : {
            BinaryLessThan* code = (BinaryLessThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparison(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThan);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOrEqualOpcodeLbl : {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparisonOrEqual(state, left, right, true));
            ADD_PROGRAM_COUNTER(BinaryLessThanOrEqual);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOpcodeLbl : {
            BinaryGreaterThan* code = (BinaryGreaterThan*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparison(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThan);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOrEqualOpcodeLbl : {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparisonOrEqual(state, right, left, false));
            ADD_PROGRAM_COUNTER(BinaryGreaterThanOrEqual);
            NEXT_INSTRUCTION();
        }

        ToNumberOpcodeLbl : {
            ToNumber* code = (ToNumber*)programCounter;
            const Value& val = registerFile[code->m_registerIndex];
            registerFile[code->m_registerIndex] = Value(val.toNumber(state));
            ;
            ADD_PROGRAM_COUNTER(ToNumber);
            NEXT_INSTRUCTION();
        }

        UnaryMinusOpcodeLbl : {
            UnaryMinus* code = (UnaryMinus*)programCounter;
            const Value& val = registerFile[code->m_registerIndex];
            registerFile[code->m_registerIndex] = Value(-val.toNumber(state));
            ADD_PROGRAM_COUNTER(UnaryMinus);
            NEXT_INSTRUCTION();
        }

        UnaryNotOpcodeLbl : {
            UnaryNot* code = (UnaryNot*)programCounter;
            const Value& val = registerFile[code->m_registerIndex];
            registerFile[code->m_registerIndex] = Value(!val.toBoolean(state));
            ADD_PROGRAM_COUNTER(UnaryNot);
            NEXT_INSTRUCTION();
        }

        GetObjectOpcodeLbl : {
            GetObject* code = (GetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_objectRegisterIndex + 1];
            PointerValue* v;
            if (LIKELY(willBeObject.isPointerValue() && (g_arrayObjectTag == *((size_t*)(v = willBeObject.asPointerValue()))))) {
                ArrayObject* arr = (ArrayObject*)v;
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx;
                    if (LIKELY(property.isUInt32()))
                        idx = property.asUInt32();
                    else {
                        idx = property.toString(state)->tryToUseAsArrayIndex();
                    }
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        ASSERT(arr->m_fastModeData.size() == arr->getArrayLength(state));
                        if (LIKELY(idx < arr->m_fastModeData.size())) {
                            const Value& v = arr->m_fastModeData[idx];
                            if (LIKELY(!v.isEmpty())) {
                                registerFile[code->m_objectRegisterIndex] = v;
                                ADD_PROGRAM_COUNTER(GetObject);
                                NEXT_INSTRUCTION();
                            }
                        }
                    }
                }
            }
            Object* obj = fastToObject(state, willBeObject);
            registerFile[code->m_objectRegisterIndex] = obj->get(state, ObjectPropertyName(state, property)).value(state, obj);
            ADD_PROGRAM_COUNTER(GetObject);
            NEXT_INSTRUCTION();
        }

        SetObjectOpcodeLbl : {
            SetObject* code = (SetObject*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            if (LIKELY(willBeObject.isPointerValue() && (g_arrayObjectTag == *((size_t*)willBeObject.asPointerValue())))) {
                ArrayObject* arr = willBeObject.asObject()->asArrayObject();
                if (LIKELY(arr->isFastModeArray())) {
                    uint32_t idx;
                    if (LIKELY(property.isUInt32()))
                        idx = property.asUInt32();
                    else {
                        idx = property.toString(state)->tryToUseAsArrayIndex();
                    }
                    if (LIKELY(idx != Value::InvalidArrayIndexValue)) {
                        uint32_t len = arr->m_fastModeData.size();
                        if (UNLIKELY(len <= idx)) {
                            if (UNLIKELY(!arr->isExtensible())) {
                                goto SetObjectOpcodeSlowCase;
                            }
                            if (UNLIKELY(!arr->setArrayLength(state, idx + 1)) || UNLIKELY(!arr->isFastModeArray())) {
                                goto SetObjectOpcodeSlowCase;
                            }
                        }
                        ASSERT(arr->m_fastModeData.size() == arr->getArrayLength(state));
                        arr->m_fastModeData[idx] = registerFile[code->m_loadRegisterIndex];
                        registerFile[code->m_objectRegisterIndex] = registerFile[code->m_loadRegisterIndex];
                        ADD_PROGRAM_COUNTER(SetObject);
                        NEXT_INSTRUCTION();
                    }
                }
            }
        SetObjectOpcodeSlowCase:
            Object* obj = willBeObject.toObject(state);
            obj->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, property), registerFile[code->m_loadRegisterIndex], obj);
            registerFile[code->m_objectRegisterIndex] = registerFile[code->m_loadRegisterIndex];
            ADD_PROGRAM_COUNTER(SetObject);
            NEXT_INSTRUCTION();
        }

        GetObjectPreComputedCaseOpcodeLbl : {
            GetObjectPreComputedCase* code = (GetObjectPreComputedCase*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            registerFile[code->m_objectRegisterIndex] = getObjectPrecomputedCaseOperation(state, fastToObject(state, willBeObject), willBeObject, code->m_propertyName, code->m_inlineCache);
            ADD_PROGRAM_COUNTER(GetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        SetObjectPreComputedCaseOpcodeLbl : {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            setObjectPreComputedCaseOperation(state, willBeObject.toObject(state), code->m_propertyName, registerFile[code->m_loadRegisterIndex], code->m_inlineCache);
            registerFile[code->m_objectRegisterIndex] = registerFile[code->m_loadRegisterIndex];
            ADD_PROGRAM_COUNTER(SetObjectPreComputedCase);
            NEXT_INSTRUCTION();
        }

        JumpOpcodeLbl : {
            Jump* code = (Jump*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            NEXT_INSTRUCTION();
        }

        JumpIfTrueOpcodeLbl : {
            JumpIfTrue* code = (JumpIfTrue*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            } else {
                ADD_PROGRAM_COUNTER(JumpIfTrue);
            }
            NEXT_INSTRUCTION();
        }

        JumpIfFalseOpcodeLbl : {
            JumpIfFalse* code = (JumpIfFalse*)programCounter;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (!registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            } else {
                ADD_PROGRAM_COUNTER(JumpIfFalse);
            }
            NEXT_INSTRUCTION();
        }

        CallFunctionOpcodeLbl : {
            CallFunction* code = (CallFunction*)programCounter;
            const Value& receiver = registerFile[code->m_registerIndex];
            const Value& callee = registerFile[code->m_registerIndex + 1];
            registerFile[code->m_registerIndex] = FunctionObject::call(state, callee, receiver, code->m_argumentCount, &registerFile[code->m_registerIndex + 2]);
            ADD_PROGRAM_COUNTER(CallFunction);
            NEXT_INSTRUCTION();
        }

        BinaryModOpcodeLbl : {
            BinaryMod* code = (BinaryMod*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = modOperation(state, left, right);
            ADD_PROGRAM_COUNTER(BinaryMod);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseAndOpcodeLbl : {
            BinaryBitwiseAnd* code = (BinaryBitwiseAnd*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) & right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseAnd);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseOrOpcodeLbl : {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) | right.toInt32(state));
            ADD_PROGRAM_COUNTER(BinaryBitwiseOr);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseXorOpcodeLbl : {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)programCounter;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) ^ right.toInt32(state));
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
            registerFile[code->m_srcIndex0] = Value(lnum);
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
            registerFile[code->m_srcIndex0] = Value(lnum);
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
            registerFile[code->m_srcIndex0] = Value(lnum);
            ADD_PROGRAM_COUNTER(BinaryUnsignedRightShift);
            NEXT_INSTRUCTION();
        }

        UnaryBitwiseNotOpcodeLbl : {
            UnaryBitwiseNot* code = (UnaryBitwiseNot*)programCounter;
            const Value& val = registerFile[code->m_registerIndex];
            registerFile[code->m_registerIndex] = Value(~val.toInt32(state));
            ADD_PROGRAM_COUNTER(UnaryBitwiseNot);
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
            arr->defineOwnProperty(state, state.context()->staticStrings().length, ObjectPropertyDescriptor(Value(code->m_length)));
            registerFile[code->m_registerIndex] = arr;
            ADD_PROGRAM_COUNTER(CreateArray);
            NEXT_INSTRUCTION();
        }

        ObjectDefineOwnPropertyOperationOpcodeLbl : {
            ObjectDefineOwnPropertyOperation* code = (ObjectDefineOwnPropertyOperation*)programCounter;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            willBeObject.asObject()->defineOwnProperty(state, ObjectPropertyName(state, property), ObjectPropertyDescriptor(registerFile[code->m_loadRegisterIndex], ObjectPropertyDescriptor::AllPresent));
            ADD_PROGRAM_COUNTER(ObjectDefineOwnPropertyOperation);
            NEXT_INSTRUCTION();
        }

        DeclareVarVariableOpcodeLbl : {
            DeclareVarVariable* code = (DeclareVarVariable*)programCounter;
            record->createMutableBinding(state, code->m_name, false);
            ADD_PROGRAM_COUNTER(DeclareVarVariable);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionExpressionOpcodeLbl : {
            DeclareFunctionExpression* code = (DeclareFunctionExpression*)programCounter;
            registerFile[code->m_registerIndex] = new FunctionObject(state, code->m_codeBlock, env);
            ADD_PROGRAM_COUNTER(DeclareFunctionExpression);
            NEXT_INSTRUCTION();
        }

        NewOperationOpcodeLbl : {
            NewOperation* code = (NewOperation*)programCounter;
            registerFile[code->m_registerIndex] = newOperation(state, registerFile[code->m_registerIndex], code->m_argumentCount, &registerFile[code->m_registerIndex + 1]);
            ADD_PROGRAM_COUNTER(NewOperation);
            NEXT_INSTRUCTION();
        }

        UnaryTypeofOpcodeLbl : {
            UnaryTypeof* code = (UnaryTypeof*)programCounter;
            Value val;
            if (code->m_id.string()->length()) {
                val = loadByName(state, env, code->m_id, false);
            } else {
                val = registerFile[code->m_registerIndex];
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

            registerFile[code->m_registerIndex] = val;
            ADD_PROGRAM_COUNTER(UnaryTypeof);
            NEXT_INSTRUCTION();
        }

        EndOpcodeLbl : {
            *state.exeuctionResult() = registerFile[0];
            return;
        }

        ReturnFunctionOpcodeLbl : {
            ReturnFunction* code = (ReturnFunction*)programCounter;
            if (code->m_registerIndex != SIZE_MAX)
                *state.exeuctionResult() = registerFile[code->m_registerIndex];
            else
                *state.exeuctionResult() = Value();
            if (UNLIKELY(state.rareData() != nullptr)) {
                if (state.rareData()->m_controlFlowRecord && state.rareData()->m_controlFlowRecord->size()) {
                    state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsReturn, Value(), state.rareData()->m_controlFlowRecord->size());
                }
            }
            return;
        }

        DeclareFunctionDeclarationOpcodeLbl : {
            DeclareFunctionDeclaration* code = (DeclareFunctionDeclaration*)programCounter;
            registerFile[0] = new FunctionObject(state, code->m_codeBlock, env);
            ADD_PROGRAM_COUNTER(DeclareFunctionDeclaration);
            NEXT_INSTRUCTION();
        }


        CallNativeFunctionOpcodeLbl : {
            CallNativeFunction* code = (CallNativeFunction*)programCounter;
            Value* argv = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->argv();
            size_t argc = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->argc();
            if (argc < record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->parametersInfomation().size()) {
                size_t len = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->parametersInfomation().size();
                Value* newArgv = ALLOCA(sizeof(Value) * len, Value, state);
                for (size_t i = 0; i < argc; i++) {
                    newArgv[i] = argv[i];
                }
                for (size_t i = argc; i < len; i++) {
                    newArgv[i] = Value();
                }
                argv = newArgv;
                // argc = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock()->parametersInfomation().size();
            }
            *state.exeuctionResult() = code->m_fn(state, env->getThisBinding(), argc, argv, record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->isNewExpression());
            return;
        }

        LoadByNameOpcodeLbl : {
            LoadByName* code = (LoadByName*)programCounter;
            registerFile[code->m_registerIndex] = loadByName(state, env, code->m_name);
            ADD_PROGRAM_COUNTER(LoadByName);
            NEXT_INSTRUCTION();
        }

        StoreByNameOpcodeLbl : {
            StoreByName* code = (StoreByName*)programCounter;
            storeByName(state, env, code->m_name, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreByName);
            NEXT_INSTRUCTION();
        }

        CallEvalFunctionOpcodeLbl : {
            CallEvalFunction* code = (CallEvalFunction*)programCounter;
            Value eval = loadByName(state, env, state.context()->staticStrings().eval);
            if (eval.equalsTo(state, state.context()->globalObject()->eval())) {
                // do eval
                Value arg;
                if (code->m_argumentCount) {
                    arg = registerFile[code->m_registerIndex];
                }
                registerFile[code->m_registerIndex] = state.context()->globalObject()->eval(state, arg, codeBlock);
            } else {
                registerFile[code->m_registerIndex] = FunctionObject::call(state, eval, Value(), code->m_argumentCount, &registerFile[code->m_registerIndex]);
            }
            ADD_PROGRAM_COUNTER(CallEvalFunction);
            NEXT_INSTRUCTION();
        }

        CallBoundFunctionOpcodeLbl : {
            CallBoundFunction* code = (CallBoundFunction*)programCounter;
            // Collect arguments info when current function is called.
            size_t calledArgc = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->argc();
            Value* calledArgv = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->argv();

            int mergedArgc = code->m_boundArgumentsCount + calledArgc;
            Value* mergedArgv = ALLOCA(mergedArgc * sizeof(Value), Value, state);
            if (code->m_boundArgumentsCount)
                memcpy(mergedArgv, code->m_boundArguments, sizeof(Value) * code->m_boundArgumentsCount);
            if (calledArgc)
                memcpy(mergedArgv + code->m_boundArgumentsCount, calledArgv, sizeof(Value) * calledArgc);

            // FIXME: consider new
            Value result = FunctionObject::call(state, code->m_boundTargetFunction, code->m_boundThis, mergedArgc, mergedArgv);
            *state.exeuctionResult() = result;
            return;
        }

        TryOperationOpcodeLbl : {
            TryOperation* code = (TryOperation*)programCounter;
            try {
                if (!state.ensureRareData()->m_controlFlowRecord) {
                    state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
                }
                state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
                size_t newPc = programCounter + sizeof(TryOperation);
                interpret(state, codeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile, stackStorage);
                programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
            } catch (const Value& val) {
                state.context()->m_sandBoxStack.back()->m_stackTraceData.clear();
                if (code->m_hasCatch == false) {
                    state.rareData()->m_controlFlowRecord->back() = new ControlFlowRecord(ControlFlowRecord::NeedsThrow, val);
                    programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
                } else {
                    // setup new env
                    EnvironmentRecord* newRecord = new DeclarativeEnvironmentRecordNotIndexed(state);
                    newRecord->createMutableBinding(state, code->m_catchVariableName);
                    newRecord->setMutableBinding(state, code->m_catchVariableName, val);
                    LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
                    ExecutionContext* newEc = new ExecutionContext(state.context(), state.executionContext(), newEnv, state.inStrictMode());

                    try {
                        ExecutionState newState(state.context(), newEc, state.exeuctionResult());
                        newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;
                        interpret(newState, codeBlock, code->m_catchPosition, registerFile, stackStorage);
                        programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
                    } catch (const Value& val) {
                        state.context()->m_sandBoxStack.back()->m_stackTraceData.clear();
                        programCounter = jumpTo(codeBuffer, code->m_tryCatchEndPosition);
                    }
                }
            }
            NEXT_INSTRUCTION();
        }

        TryCatchWithBodyEndOpcodeLbl : {
            (*(state.rareData()->m_controlFlowRecord))[state.rareData()->m_controlFlowRecord->size() - 1] = nullptr;
            return;
        }

        FinallyEndOpcodeLbl : {
            FinallyEnd* code = (FinallyEnd*)programCounter;
            ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
            state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);
            if (record) {
                if (record->reason() == ControlFlowRecord::NeedsJump) {
                    size_t pos = (size_t)record->value().asPointerValue();
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                        return;
                    } else
                        programCounter = jumpTo(codeBuffer, pos);
                } else if (record->reason() == ControlFlowRecord::NeedsThrow) {
                    throw record->value();
                } else if (record->reason() == ControlFlowRecord::NeedsReturn) {
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                    }
                    return;
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
            if (!state.ensureRareData()->m_controlFlowRecord) {
                state.ensureRareData()->m_controlFlowRecord = new ControlFlowRecordVector();
            }
            state.ensureRareData()->m_controlFlowRecord->pushBack(nullptr);
            size_t newPc = programCounter + sizeof(WithOperation);

            // setup new env
            EnvironmentRecord* newRecord = new ObjectEnvironmentRecord(state, registerFile[code->m_registerIndex].toObject(state));
            LexicalEnvironment* newEnv = new LexicalEnvironment(newRecord, env);
            ExecutionContext* newEc = new ExecutionContext(state.context(), state.executionContext(), newEnv, state.inStrictMode());
            ExecutionState newState(state.context(), newEc, state.exeuctionResult());
            newState.ensureRareData()->m_controlFlowRecord = state.rareData()->m_controlFlowRecord;

            interpret(newState, codeBlock, resolveProgramCounter(codeBuffer, newPc), registerFile, stackStorage);

            ControlFlowRecord* record = state.rareData()->m_controlFlowRecord->back();
            state.rareData()->m_controlFlowRecord->erase(state.rareData()->m_controlFlowRecord->size() - 1);

            if (record) {
                if (record->reason() == ControlFlowRecord::NeedsJump) {
                    size_t pos = (size_t)record->value().asPointerValue();
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                        return;
                    } else
                        programCounter = jumpTo(codeBuffer, pos);
                } else {
                    ASSERT(record->reason() == ControlFlowRecord::NeedsReturn);
                    record->m_count--;
                    if (record->count()) {
                        state.rareData()->m_controlFlowRecord->back() = record;
                    }
                    return;
                }
            } else {
                programCounter = jumpTo(codeBuffer, code->m_withEndPostion);
            }
            NEXT_INSTRUCTION();
        }

        JumpComplexCaseOpcodeLbl : {
            JumpComplexCase* code = (JumpComplexCase*)programCounter;
            state.ensureRareData()->m_controlFlowRecord->back() = code->m_controlFlowRecord->clone();
            return;
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
                testItem.m_version = obj->structure()->version();
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

            if (shouldUpdateEnumerateObjectData) {
                registerFile[code->m_registerIndex] = Value((PointerValue*)updateEnumerateObjectData(state, data));
            }

            if (data->m_keys.size() == data->m_idx) {
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
            registerFile[code->m_registerIndex] = data->m_keys[data->m_idx - 1].toString(state);
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
            if (code->m_id.string()->length()) {
                bool result;
                if (UNLIKELY(code->m_id == state.context()->staticStrings().arguments && !env->record()->isGlobalEnvironmentRecord())) {
                    result = false;
                } else {
                    result = env->deleteBinding(state, code->m_id);
                }
                registerFile[code->m_registerIndex0] = Value(result);
            } else {
                const Value& o = registerFile[code->m_registerIndex0];
                const Value& p = registerFile[code->m_registerIndex1];
                Object* obj = o.toObject(state);
                bool result = obj->deleteOwnProperty(state, ObjectPropertyName(state, p));
                if (!result && state.inStrictMode())
                    Object::throwCannotDeleteError(state, ObjectPropertyName(state, p).toPropertyName(state));
                registerFile[code->m_registerIndex0] = Value(result);
            }
            ADD_PROGRAM_COUNTER(UnaryDelete);
            NEXT_INSTRUCTION();
        }

        BinaryInOperationOpcodeLbl : {
            BinaryInOperation* code = (BinaryInOperation*)programCounter;
            if (!registerFile[code->m_srcIndex1].isObject())
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "type of rvalue is not Object");
            auto result = registerFile[code->m_srcIndex1].toObject(state)->get(state, ObjectPropertyName(state, registerFile[code->m_srcIndex0]));
            registerFile[code->m_srcIndex0] = Value(result.hasValue());
            ADD_PROGRAM_COUNTER(BinaryInOperation);
            NEXT_INSTRUCTION();
        }

        BinaryInstanceOfOperationOpcodeLbl : {
            BinaryInstanceOfOperation* code = (BinaryInstanceOfOperation*)programCounter;
            registerFile[code->m_srcIndex0] = instanceOfOperation(state, registerFile[code->m_srcIndex0], registerFile[code->m_srcIndex1]);
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

        LoadArgumentsObjectOpcodeLbl : {
            LoadArgumentsObject* code = (LoadArgumentsObject*)programCounter;
            registerFile[code->m_registerIndex] = loadArgumentsObject(state, ec);
            ADD_PROGRAM_COUNTER(LoadArgumentsObject);
            NEXT_INSTRUCTION();
        }

        StoreArgumentsObjectOpcodeLbl : {
            StoreArgumentsObject* code = (StoreArgumentsObject*)programCounter;
            ExecutionContext* e = ec;
            while (!(e->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && e->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord())) {
                e = e->parent();
            }
            auto fnRecord = e->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            fnRecord->m_isArgumentObjectCreated = true;
            auto result = fnRecord->hasBinding(state, state.context()->staticStrings().arguments);
            fnRecord->setMutableBindingByIndex(state, result.m_index, state.context()->staticStrings().arguments, registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(StoreArgumentsObject);
            NEXT_INSTRUCTION();
        }

        ResetExecuteResultOpcodeLbl : {
            registerFile[0] = Value();
            ADD_PROGRAM_COUNTER(ResetExecuteResult);
            NEXT_INSTRUCTION();
        }

        CallFunctionInWithScopeOpcodeLbl : {
            CallFunctionInWithScope* code = (CallFunctionInWithScope*)programCounter;
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
            registerFile[code->m_registerIndex] = FunctionObject::call(state, callee, receiverObj, code->m_argumentCount, &registerFile[code->m_registerIndex]);
            ADD_PROGRAM_COUNTER(CallFunctionInWithScope);
            NEXT_INSTRUCTION();
        }

        LoadArgumentsInWithScopeOpcodeLbl : {
            LoadArgumentsObject* code = (LoadArgumentsObject*)programCounter;
            Value val;
            LexicalEnvironment* envTmp = env;
            while (envTmp) {
                if (envTmp->record()->isObjectEnvironmentRecord()) {
                    EnvironmentRecord::GetBindingValueResult result = envTmp->record()->getBindingValue(state, state.context()->staticStrings().arguments);
                    if (result.m_hasBindingValue)
                        val = result.m_value;
                    else
                        val = loadArgumentsObject(state, ec);
                    break;
                } else if (envTmp->record()->isDeclarativeEnvironmentRecord() && envTmp->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                    val = loadArgumentsObject(state, ec);
                    break;
                }
                envTmp = envTmp->outerEnvironment();
            }
            registerFile[code->m_registerIndex] = val;
            loadArgumentsObject(state, ec);
            ADD_PROGRAM_COUNTER(LoadArgumentsObject);
            NEXT_INSTRUCTION();
        }

        } catch (const Value& v) {
            if (state.context()->m_sandBoxStack.size()) {
                SandBox* sb = state.context()->m_sandBoxStack.back();
                if (ec->m_lexicalEnvironment->record()->isDeclarativeEnvironmentRecord()) {
                    if (ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                        FunctionObject* fn = ec->m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
                        CodeBlock* cb = fn->codeBlock();
                        ByteCodeBlock* b = cb->byteCodeBlock();
                        ByteCode* code = b->peekCode<ByteCode>(programCounter - (size_t)b->m_code.data());
                        ExtendedNodeLOC loc = b->computeNodeLOCFromByteCode(code, cb);
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
                    ByteCode* code = b->peekCode<ByteCode>((size_t)programCounter - (size_t)b->m_code.data());
                    ExtendedNodeLOC loc = b->computeNodeLOCFromByteCode(code, cb);
                    SandBox::StackTraceData data;
                    data.loc = loc;
                    data.fileName = cb->script()->fileName();
                    sb->m_stackTraceData.pushBack(std::make_pair(ec, data));
                }
            }
            throw v;
        }
    }
FillOpcodeTable : {
#define REGISTER_TABLE(opcode, pushCount, popCount) \
    registerOpcode(opcode##Opcode, &&opcode##OpcodeLbl);
    FOR_EACH_BYTECODE_OP(REGISTER_TABLE);

#undef REGISTER_TABLE
}
}

NEVER_INLINE Value ByteCodeInterpreter::loadArgumentsObject(ExecutionState& state, ExecutionContext* e)
{
    while (!(e->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && e->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord())) {
        e = e->parent();
    }
    Value val(Value::ForceUninitialized);
    auto fnRecord = e->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
    if (e->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->isArgumentObjectCreated()) {
        val = fnRecord->getBindingValue(state, state.context()->staticStrings().arguments).m_value;
    } else {
        val = fnRecord->createArgumentsObject(state, e);
        auto result = fnRecord->hasBinding(state, state.context()->staticStrings().arguments);
        fnRecord->setMutableBindingByIndex(state, result.m_index, state.context()->staticStrings().arguments, val);
    }
    return val;
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
        ret = RopeString::createRopeString(lval.toString(state), rval.toString(state));
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

NEVER_INLINE Value ByteCodeInterpreter::newOperation(ExecutionState& state, const Value& callee, size_t argc, Value* argv)
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
    if (cb->isNativeFunction()) {
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
        return res;
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
        // while (C->isBoundFunc())
        //     C = C->codeBlock()->peekCode<CallBoundFunction>(0)->m_boundTargetFunction;
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

ALWAYS_INLINE Value ByteCodeInterpreter::getObjectPrecomputedCaseOperation(ExecutionState& state, Object* obj, const Value& target, const PropertyName& name, GetObjectInlineCache& inlineCache)
{
    unsigned currentCacheIndex = 0;
    ObjectStructureChainItem testItem;
    testItem.m_objectStructure = obj->structure();
    testItem.m_version = obj->structure()->version();
    const size_t cacheFillCount = inlineCache.m_cache.size();
TestCache:
    for (; currentCacheIndex < cacheFillCount; currentCacheIndex++) {
        const GetObjectInlineCacheData& data = inlineCache.m_cache[currentCacheIndex];
        const ObjectStructureChain* const cachedHiddenClassChain = &data.m_cachedhiddenClassChain;
        const size_t& cachedIndex = data.m_cachedIndex;
        const size_t cSiz = cachedHiddenClassChain->size() - 1;
        for (size_t i = 0; i < cSiz; i++) {
            testItem.m_objectStructure = obj->structure();
            testItem.m_version = obj->structure()->version();
            if (UNLIKELY((*cachedHiddenClassChain)[i] != testItem)) {
                currentCacheIndex++;
                goto TestCache;
            }
            Object* protoObject = obj->getPrototypeObject();
            if (LIKELY(protoObject != nullptr)) {
                obj = protoObject;
            } else {
                currentCacheIndex++;
                goto TestCache;
            }
        }

        testItem.m_objectStructure = obj->structure();
        testItem.m_version = obj->structure()->version();
        if (LIKELY((*cachedHiddenClassChain)[cSiz] == testItem)) {
            if (LIKELY(cachedIndex != SIZE_MAX)) {
                return obj->getOwnPropertyUtilForObject(state, cachedIndex, target);
            } else {
                return Value();
            }
        }
    }

    return getObjectPrecomputedCaseOperationCacheMiss(state, obj, target, name, inlineCache);
}

NEVER_INLINE Value ByteCodeInterpreter::getObjectPrecomputedCaseOperationCacheMiss(ExecutionState& state, Object* obj, const Value& target, const PropertyName& name, GetObjectInlineCache& inlineCache)
{
    // cache miss.
    inlineCache.m_executeCount++;
    inlineCache.m_cacheMissCount++;
    if (inlineCache.m_executeCount <= 3 || inlineCache.m_cacheMissCount > 16) {
        inlineCache.m_cache.clear();
        return obj->get(state, ObjectPropertyName(state, name)).value(state, target);
    }

    inlineCache.m_cache.insert(0, GetObjectInlineCacheData());
    if (inlineCache.m_cache.size() > 6) {
        inlineCache.m_cache.erase(5);
    }
    ObjectStructureChain* cachedHiddenClassChain = &inlineCache.m_cache[0].m_cachedhiddenClassChain;
    size_t* cachedHiddenClassIndex = &inlineCache.m_cache[0].m_cachedIndex;

    ObjectStructureChainItem newItem;
    while (true) {
        newItem.m_objectStructure = obj->structure();
        newItem.m_version = obj->structure()->version();

        cachedHiddenClassChain->push_back(newItem);
        size_t idx = obj->structure()->findProperty(state, name);
        if (idx != SIZE_MAX) {
            *cachedHiddenClassIndex = idx;
            break;
        }
        const Value& proto = obj->getPrototype(state);
        if (proto.isObject()) {
            obj = proto.asObject();
        } else
            break;
    }

    if (*cachedHiddenClassIndex != SIZE_MAX) {
        return obj->getOwnPropertyUtilForObject(state, *cachedHiddenClassIndex, target);
    } else {
        return Value();
    }
}

ALWAYS_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperation(ExecutionState& state, Object* obj, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache)
{
    Object* originalObject = obj;

    ObjectStructureChainItem testItem;
    testItem.m_objectStructure = obj->structure();
    testItem.m_version = obj->structure()->version();

    if (inlineCache.m_cachedIndex != SIZE_MAX && inlineCache.m_cachedhiddenClassChain[0] == testItem) {
        ASSERT(inlineCache.m_cachedhiddenClassChain.size() == 1);
        // cache hit!
        obj->setOwnPropertyThrowsExceptionWhenStrictMode(state, inlineCache.m_cachedIndex, value);
        return;
    } else if (inlineCache.m_hiddenClassWillBe) {
        int cSiz = inlineCache.m_cachedhiddenClassChain.size();
        bool miss = false;
        for (int i = 0; i < cSiz - 1; i++) {
            testItem.m_objectStructure = obj->structure();
            testItem.m_version = obj->structure()->version();
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
                obj->m_values.push_back(value);
                obj->m_structure = inlineCache.m_hiddenClassWillBe;
                return;
            }
        }
    }

    setObjectPreComputedCaseOperationCacheMiss(state, originalObject, name, value, inlineCache);
}

NEVER_INLINE void ByteCodeInterpreter::setObjectPreComputedCaseOperationCacheMiss(ExecutionState& state, Object* originalObject, const PropertyName& name, const Value& value, SetObjectInlineCache& inlineCache)
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
        newItem.m_version = obj->structure()->version();

        obj->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value);

        inlineCache.m_cachedIndex = idx;
        inlineCache.m_cachedhiddenClassChain.push_back(newItem);
    } else {
        Object* orgObject = obj;
        if (UNLIKELY(obj->structure()->isStructureWithFastAccess())) {
            inlineCache.invalidateCache();
            orgObject->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, name), value, orgObject);
            return;
        }

        ASSERT(obj->structure()->version() == 0);
        ObjectStructureChainItem newItem;
        newItem.m_objectStructure = obj->structure();
        newItem.m_version = obj->structure()->version();
        inlineCache.m_cachedhiddenClassChain.push_back(newItem);
        Value proto = obj->getPrototype(state);
        while (proto.isObject()) {
            obj = proto.asObject();
            newItem.m_objectStructure = obj->structure();
            newItem.m_version = obj->structure()->version();
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

    Value target = data->m_object;

    size_t ownKeyCount = 0;
    bool shouldSearchProto = false;

    target.asObject()->enumeration(state, [&](const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc) -> bool {
        if (desc.isEnumerable()) {
            ownKeyCount++;
        }
        return true;
    });
    ObjectStructureChainItem newItem;
    newItem.m_objectStructure = target.asObject()->structure();
    newItem.m_version = target.asObject()->structure()->version();

    data->m_hiddenClassChain.push_back(newItem);

    std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, gc_malloc_ignore_off_page_allocator<String*>> keyStringSet;

    target = target.asObject()->getPrototype(state);
    while (target.isObject()) {
        if (!shouldSearchProto) {
            target.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) -> bool {
                if (desc.isEnumerable()) {
                    shouldSearchProto = true;
                    return false;
                }
                return true;
            });
        }
        newItem.m_objectStructure = target.asObject()->structure();
        newItem.m_version = target.asObject()->structure()->version();
        data->m_hiddenClassChain.push_back(newItem);
        target = target.asObject()->getPrototype(state);
    }

    target = obj;
    if (shouldSearchProto) {
        while (target.isObject()) {
            target.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) -> bool {
                if (desc.isEnumerable()) {
                    String* key = name.toValue(state).toString(state);
                    auto iter = keyStringSet.find(key);
                    if (iter == keyStringSet.end()) {
                        keyStringSet.insert(key);
                        data->m_keys.pushBack(name.toValue(state));
                    }
                } else if (target == obj) {
                    // 12.6.4 The values of [[Enumerable]] attributes are not considered
                    // when determining if a property of a prototype object is shadowed by a previous object on the prototype chain.
                    String* key = name.toValue(state).toString(state);
                    ASSERT(keyStringSet.find(key) == keyStringSet.end());
                    keyStringSet.insert(key);
                }
                return true;
            });
            target = target.asObject()->getPrototype(state);
        }
    } else {
        size_t idx = 0;
        data->m_keys.resizeWithUninitializedValues(ownKeyCount);
        target.asObject()->enumeration(state, [&](const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc) -> bool {
            if (desc.isEnumerable()) {
                data->m_keys[idx++] = name.toValue(state);
            }
            return true;
        });
        ASSERT(ownKeyCount == idx);
    }

    return data;
}

NEVER_INLINE EnumerateObjectData* ByteCodeInterpreter::updateEnumerateObjectData(ExecutionState& state, EnumerateObjectData* data)
{
    EnumerateObjectData* newData = executeEnumerateObject(state, data->m_object);
    std::vector<Value, gc_malloc_ignore_off_page_allocator<Value>> oldKeys;
    if (data->m_keys.size()) {
        oldKeys.insert(oldKeys.end(), &data->m_keys[0], &data->m_keys[data->m_keys.size() - 1] + 1);
    }
    std::vector<Value, gc_malloc_ignore_off_page_allocator<Value>> differenceKeys;
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
    if (LIKELY(obj.isPointerValue())) {
        PointerValue* v = obj.asPointerValue();
        if (LIKELY(v->isObject())) {
            return obj.asObject();
        } else if (v->isString()) {
            StringObject* o = state.context()->globalObject()->stringProxyObject();
            o->setPrimitiveValue(state, obj.asString());
            return o;
        }
    } else if (obj.isNumber()) {
        NumberObject* o = state.context()->globalObject()->numberProxyObject();
        o->setPrimitiveValue(state, obj.asNumber());
        return o;
    }
    return obj.toObject(state);
}

NEVER_INLINE Value ByteCodeInterpreter::getGlobalObjectSlowCase(ExecutionState& state, Object* go, GetGlobalObject* code)
{
    size_t idx = go->structure()->findProperty(state, code->m_propertyName);
    if (UNLIKELY(idx == SIZE_MAX)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.string(), false, String::emptyString, errorMessage_IsNotDefined);
    } else {
        const ObjectStructureItem& item = go->structure()->readProperty(state, idx);
        if (!item.m_descriptor.isPlainDataProperty()) {
            code->m_savedGlobalObjectVersion = SIZE_MAX;
            return go->getOwnPropertyUtilForObject(state, idx, go);
        }

        code->m_cachedIndex = idx;
        code->m_savedGlobalObjectVersion = go->structure()->version();
    }
    return go->getOwnPropertyUtilForObject(state, idx, go);
}

NEVER_INLINE void ByteCodeInterpreter::setGlobalObjectSlowCase(ExecutionState& state, Object* go, SetGlobalObject* code, const Value& value)
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
            code->m_savedGlobalObjectVersion = SIZE_MAX;
            go->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, code->m_propertyName), value, go);
            return;
        }
        code->m_cachedIndex = idx;
        code->m_savedGlobalObjectVersion = go->structure()->version();
        go->setOwnPropertyThrowsExceptionWhenStrictMode(state, idx, value);
    }
}
}
