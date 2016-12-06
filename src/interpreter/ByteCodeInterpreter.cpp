#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/FunctionObject.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"
#include "runtime/GlobalObject.h"
#include "runtime/ErrorObject.h"
#include "../third_party/checked_arithmetic/CheckedArithmetic.h"

namespace Escargot {

NEVER_INLINE void registerOpcode(Opcode opcode, void* opcodeAddress)
{
    static std::unordered_set<void *> labelAddressChecker;
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

template <typename CodeType>
ALWAYS_INLINE void executeNextCode(size_t& programCounter)
{
    programCounter += sizeof(CodeType);
}

ALWAYS_INLINE size_t jumpTo(char* codeBuffer, const size_t& jumpPosition)
{
    return (size_t)&codeBuffer[jumpPosition];
}


void ByteCodeInterpreter::interpret(ExecutionState& state, CodeBlock* codeBlock)
{
    if (UNLIKELY(codeBlock == nullptr)) {
        goto FillOpcodeTable;
    }
    {
        ASSERT(codeBlock->byteCodeBlock() != nullptr);
        ByteCodeBlock* byteCodeBlock = codeBlock->byteCodeBlock();
        ExecutionContext* ec = state.executionContext();
        LexicalEnvironment* env = ec->lexicalEnvironment();
        Value* registerFile = ALLOCA(byteCodeBlock->m_requiredRegisterFileSizeInValueSize * sizeof(Value), Value, state);
        Value* stackStorage = ec->stackStorage();
        char* codeBuffer = byteCodeBlock->m_code.data();
        size_t& programCounter = ec->programCounter();
        programCounter = (size_t)(&codeBuffer[0]);
        ByteCode* currentCode;

#define NEXT_INSTRUCTION() \
        currentCode = (ByteCode *)programCounter; \
        ASSERT(((size_t)currentCode % sizeof(size_t)) == 0); \
        goto *(currentCode->m_opcodeInAddress);

        currentCode = (ByteCode *)programCounter;
        ASSERT(((size_t)currentCode % sizeof(size_t)) == 0);
        goto *(currentCode->m_opcodeInAddress);

        LoadLiteralOpcodeLbl:
        {
            LoadLiteral* code = (LoadLiteral*)currentCode;
            registerFile[code->m_registerIndex] = code->m_value;
            executeNextCode<LoadLiteral>(programCounter);
            NEXT_INSTRUCTION();
        }

        MoveOpcodeLbl:
        {
            Move* code = (Move*)currentCode;
            registerFile[code->m_registerIndex1] = registerFile[code->m_registerIndex0];
            executeNextCode<Move>(programCounter);
            NEXT_INSTRUCTION();
        }

        LoadByStackIndexOpcodeLbl:
        {
            LoadByStackIndex* code = (LoadByStackIndex*)currentCode;
            registerFile[code->m_registerIndex] = stackStorage[code->m_index];
            executeNextCode<LoadByStackIndex>(programCounter);
            NEXT_INSTRUCTION();
        }

        StoreByStackIndexOpcodeLbl:
        {
            StoreByStackIndex* code = (StoreByStackIndex*)currentCode;
            stackStorage[code->m_index] = registerFile[code->m_registerIndex];
            executeNextCode<StoreByStackIndex>(programCounter);
            NEXT_INSTRUCTION();
        }

        LoadByHeapIndexOpcodeLbl:
        {
            LoadByHeapIndex* code = (LoadByHeapIndex*)currentCode;
            LexicalEnvironment* upperEnv = env;
            for (size_t i = 0; i < code->m_upperIndex; i ++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            registerFile[code->m_registerIndex] = ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index];
            executeNextCode<LoadByHeapIndex>(programCounter);
            NEXT_INSTRUCTION();
        }

        StoreByHeapIndexOpcodeLbl:
        {
            StoreByHeapIndex* code = (StoreByHeapIndex*)currentCode;
            LexicalEnvironment* upperEnv = env;
            for (size_t i = 0; i < code->m_upperIndex; i ++) {
                upperEnv = upperEnv->outerEnvironment();
            }
            FunctionEnvironmentRecord* record = upperEnv->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            ASSERT(record->isFunctionEnvironmentRecordOnHeap());
            ((FunctionEnvironmentRecordOnHeap*)record)->m_heapStorage[code->m_index] = registerFile[code->m_registerIndex];
            executeNextCode<StoreByHeapIndex>(programCounter);
            NEXT_INSTRUCTION();
        }

        LoadByGlobalNameOpcodeLbl:
        {
            LoadByGlobalName* code = (LoadByGlobalName*)currentCode;
            GlobalObject* g = state.context()->globalObject();
            // check cache
            if (UNLIKELY(!g->hasPropertyOnIndex(state, code->m_name, code->m_cacheIndex))) {
                // fill cache
                code->m_cacheIndex = g->findPropertyIndex(state, code->m_name);
                ASSERT(code->m_cacheIndex != SIZE_MAX);
            }
            registerFile[code->m_registerIndex] = g->getPropertyOnIndex(state, code->m_cacheIndex);
            executeNextCode<LoadByGlobalName>(programCounter);
            NEXT_INSTRUCTION();
        }

        StoreByGlobalNameOpcodeLbl:
        {
            StoreByGlobalName* code = (StoreByGlobalName*)currentCode;
            GlobalObject* g = state.context()->globalObject();
            // check cache
            if (UNLIKELY(!g->hasPropertyOnIndex(state, code->m_name, code->m_cacheIndex))) {
                // fill cache
                code->m_cacheIndex = g->findPropertyIndex(state, code->m_name);
                ASSERT(code->m_cacheIndex != SIZE_MAX);
            }
            if (UNLIKELY(!g->setPropertyOnIndex(state, code->m_cacheIndex, registerFile[code->m_registerIndex]))) {
                if (state.inStrictMode()) {
                    // TODO throw execption
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
            executeNextCode<StoreByGlobalName>(programCounter);
            NEXT_INSTRUCTION();
        }

        LoadByNameOpcodeLbl:
        {
            LoadByName* code = (LoadByName*)currentCode;
            registerFile[code->m_registerIndex] = loadByName(state, env, code->m_name);
            executeNextCode<LoadByName>(programCounter);
            NEXT_INSTRUCTION();
        }

        StoreByNameOpcodeLbl:
        {
            StoreByName* code = (StoreByName*)currentCode;
            env->record()->setMutableBinding(state, code->m_name, registerFile[code->m_registerIndex]);
            executeNextCode<StoreByName>(programCounter);
            NEXT_INSTRUCTION();
        }

        StoreExecutionResultOpcodeLbl:
        {
            StoreExecutionResult* code = (StoreExecutionResult*)currentCode;
            *state.exeuctionResult() = registerFile[code->m_registerIndex];
            executeNextCode<StoreExecutionResult>(programCounter);
            NEXT_INSTRUCTION();
        }

        DeclareVarVariableOpcodeLbl:
        {
            DeclareVarVariable* code = (DeclareVarVariable*)currentCode;
            env->record()->createMutableBinding(state, code->m_name, false);
            executeNextCode<DeclareVarVariable>(programCounter);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionExpressionOpcodeLbl:
        {
            DeclareFunctionExpression* code = (DeclareFunctionExpression*)currentCode;
            registerFile[code->m_registerIndex] = new FunctionObject(state, code->m_codeBlock);
            executeNextCode<DeclareFunctionExpression>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryPlusOpcodeLbl:
        {
            BinaryPlus* code = (BinaryPlus*)currentCode;
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
            executeNextCode<BinaryPlus>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryMinusOpcodeLbl:
        {
            BinaryMinus* code = (BinaryMinus*)currentCode;
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
            executeNextCode<BinaryMinus>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryMultiplyOpcodeLbl:
        {
            BinaryMultiply* code = (BinaryMultiply*)currentCode;
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
            executeNextCode<BinaryMultiply>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryDivisionOpcodeLbl:
        {
            BinaryDivision* code = (BinaryDivision*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toNumber(state) / right.toNumber(state));
            executeNextCode<BinaryDivision>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryModOpcodeLbl:
        {
            BinaryMod* code = (BinaryMod*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = modOperation(state, left, right);
            executeNextCode<BinaryMod>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryEqualOpcodeLbl:
        {
            BinaryEqual* code = (BinaryEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.abstractEqualsTo(state, right));
            executeNextCode<BinaryEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryNotEqualOpcodeLbl:
        {
            BinaryNotEqual* code = (BinaryNotEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(!left.abstractEqualsTo(state, right));
            executeNextCode<BinaryNotEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryStrictEqualOpcodeLbl:
        {
            BinaryStrictEqual* code = (BinaryStrictEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.equalsTo(state, right));
            executeNextCode<BinaryStrictEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryNotStrictEqualOpcodeLbl:
        {
            BinaryNotStrictEqual* code = (BinaryNotStrictEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(!left.equalsTo(state, right));
            executeNextCode<BinaryNotStrictEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOpcodeLbl:
        {
            BinaryLessThan* code = (BinaryLessThan*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparison(state, left, right, true));
            executeNextCode<BinaryLessThan>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryLessThanOrEqualOpcodeLbl:
        {
            BinaryLessThanOrEqual* code = (BinaryLessThanOrEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparisonOrEqual(state, left, right, true));
            executeNextCode<BinaryLessThanOrEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOpcodeLbl:
        {
            BinaryGreaterThan* code = (BinaryGreaterThan*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparison(state, right, left, false));
            executeNextCode<BinaryGreaterThan>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryGreaterThanOrEqualOpcodeLbl:
        {
            BinaryGreaterThanOrEqual* code = (BinaryGreaterThanOrEqual*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(abstractRelationalComparisonOrEqual(state, right, left, false));
            executeNextCode<BinaryGreaterThanOrEqual>(programCounter);
            NEXT_INSTRUCTION();
        }

        IncrementOpcodeLbl:
        {
            Increment* code = (Increment*)currentCode;
            const Value& val = registerFile[code->m_registerIndex];
            Value ret(Value::ForceUninitialized);
            if (LIKELY(val.isInt32())) {
                int32_t a = val.asInt32();
                if (UNLIKELY(a == std::numeric_limits<int32_t>::max()))
                    ret = Value(Value::EncodeAsDouble, ((double)a) + 1);
                else
                    ret = Value(a + 1);
            } else {
                ret = Value(val.toNumber(state) + 1);
            }
            registerFile[code->m_registerIndex] = ret;
            executeNextCode<Increment>(programCounter);
            NEXT_INSTRUCTION();
        }

        DecrementOpcodeLbl:
        {
            Decrement* code = (Decrement*)currentCode;
            const Value& val = registerFile[code->m_registerIndex];
            Value ret(Value::ForceUninitialized);
            if (LIKELY(val.isInt32())) {
                int32_t a = val.asInt32();
                if (UNLIKELY(a == std::numeric_limits<int32_t>::min()))
                    ret = Value(Value::EncodeAsDouble, ((double)a) - 1);
                else
                    ret = Value(a - 1);
            } else {
                ret = Value(val.toNumber(state) - 1);
            }
            registerFile[code->m_registerIndex] = ret;
            executeNextCode<Decrement>(programCounter);
            NEXT_INSTRUCTION();
        }

        GetObjectOpcodeLbl:
        {
            GetObject* code = (GetObject*)currentCode;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_objectRegisterIndex + 1];
            registerFile[code->m_objectRegisterIndex] = willBeObject.toObject(state)->get(state, property.toString(state)).m_value;
            executeNextCode<GetObject>(programCounter);
            NEXT_INSTRUCTION();
        }

        SetObjectOpcodeLbl:
        {
            SetObject* code = (SetObject*)currentCode;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            const Value& property = registerFile[code->m_propertyRegisterIndex];
            willBeObject.toObject(state)->set(state, property.toString(state), registerFile[code->m_loadRegisterIndex]);
            executeNextCode<SetObject>(programCounter);
            NEXT_INSTRUCTION();
        }

        GetObjectPreComputedCaseOpcodeLbl:
        {
            GetObjectPreComputedCase* code = (GetObjectPreComputedCase*)currentCode;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            registerFile[code->m_objectRegisterIndex] = willBeObject.toObject(state)->get(state, code->m_propertyName).m_value;
            executeNextCode<GetObjectPreComputedCase>(programCounter);
            NEXT_INSTRUCTION();
        }

        SetObjectPreComputedCaseOpcodeLbl:
        {
            SetObjectPreComputedCase* code = (SetObjectPreComputedCase*)currentCode;
            const Value& willBeObject = registerFile[code->m_objectRegisterIndex];
            willBeObject.toObject(state)->set(state, code->m_propertyName, registerFile[code->m_loadRegisterIndex]);
            executeNextCode<SetObjectPreComputedCase>(programCounter);
            NEXT_INSTRUCTION();
        }

        GetGlobalObjectOpcodeLbl:
        {
            GetGlobalObject* code = (GetGlobalObject*)currentCode;
            auto result = state.context()->globalObject()->get(state, code->m_propertyName);
            if (UNLIKELY(!result.m_hasValue)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, code->m_propertyName.string(), false, String::emptyString, errorMessage_IsNotDefined);
            }
            registerFile[code->m_registerIndex] = result.m_value;
            executeNextCode<GetGlobalObject>(programCounter);
            NEXT_INSTRUCTION();
        }

        SetGlobalObjectOpcodeLbl:
        {
            SetGlobalObject* code = (SetGlobalObject*)currentCode;
            state.context()->globalObject()->set(state, code->m_propertyName, registerFile[code->m_registerIndex]);
            executeNextCode<SetGlobalObject>(programCounter);
            NEXT_INSTRUCTION();
        }

        JumpOpcodeLbl:
        {
            Jump* code = (Jump *)currentCode;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            NEXT_INSTRUCTION();
        }

        JumpIfTrueOpcodeLbl:
        {
            JumpIfTrue* code = (JumpIfTrue*)currentCode;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            } else {
                executeNextCode<JumpIfTrue>(programCounter);
            }
            NEXT_INSTRUCTION();
        }

        JumpIfFalseOpcodeLbl:
        {
            JumpIfFalse* code = (JumpIfFalse*)currentCode;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            if (!registerFile[code->m_registerIndex].toBoolean(state)) {
                programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            } else {
                executeNextCode<JumpIfFalse>(programCounter);
            }
            NEXT_INSTRUCTION();
        }

        CallFunctionOpcodeLbl:
        {
            CallFunction* code = (CallFunction*)currentCode;
            const Value& receiver = registerFile[code->m_registerIndex];
            const Value& callee = registerFile[code->m_registerIndex + 1];
            registerFile[code->m_registerIndex] = FunctionObject::call(callee, state, receiver, code->m_argumentCount, &registerFile[code->m_registerIndex + 2]);
            executeNextCode<CallFunction>(programCounter);
            NEXT_INSTRUCTION();
        }

        DeclareFunctionDeclarationOpcodeLbl:
        {
            DeclareFunctionDeclaration* code = (DeclareFunctionDeclaration*)currentCode;
            registerFile[0] = new FunctionObject(state, code->m_codeBlock);
            executeNextCode<DeclareFunctionDeclaration>(programCounter);
            NEXT_INSTRUCTION();
        }

        ReturnFunctionOpcodeLbl:
        {
            ReturnFunction* code = (ReturnFunction*)currentCode;
            *state.exeuctionResult() = registerFile[code->m_registerIndex];
            return;
        }

        BinaryBitwiseAndOpcodeLbl:
        {
            BinaryBitwiseAnd* code = (BinaryBitwiseAnd*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) & right.toInt32(state));
            executeNextCode<BinaryBitwiseAnd>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseOrOpcodeLbl:
        {
            BinaryBitwiseOr* code = (BinaryBitwiseOr*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) | right.toInt32(state));
            executeNextCode<BinaryBitwiseOr>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryBitwiseXorOpcodeLbl:
        {
            BinaryBitwiseXor* code = (BinaryBitwiseXor*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            registerFile[code->m_srcIndex0] = Value(left.toInt32(state) ^ right.toInt32(state));
            executeNextCode<BinaryBitwiseXor>(programCounter);
            NEXT_INSTRUCTION();
        }

        BinaryLeftShiftOpcodeLbl:
        {
            BinaryLeftShift* code = (BinaryLeftShift*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            int32_t lnum = left.toInt32(state);
            int32_t rnum = right.toInt32(state);
            lnum <<= ((unsigned int)rnum) & 0x1F;
            registerFile[code->m_srcIndex0] = Value(lnum);
            executeNextCode<BinaryLeftShift>(programCounter);
            NEXT_INSTRUCTION();
        }


        BinarySignedRightShiftOpcodeLbl:
        {
            BinarySignedRightShift* code = (BinarySignedRightShift*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            int32_t lnum = left.toInt32(state);
            int32_t rnum = right.toInt32(state);
            lnum >>= ((unsigned int)rnum) & 0x1F;
            registerFile[code->m_srcIndex0] = Value(lnum);
            executeNextCode<BinarySignedRightShift>(programCounter);
            NEXT_INSTRUCTION();
        }


        BinaryUnsignedRightShiftOpcodeLbl:
        {
            BinaryUnsignedRightShift* code = (BinaryUnsignedRightShift*)currentCode;
            const Value& left = registerFile[code->m_srcIndex0];
            const Value& right = registerFile[code->m_srcIndex1];
            uint32_t lnum = left.toUint32(state);
            uint32_t rnum = right.toUint32(state);
            lnum = (lnum) >> ((rnum) & 0x1F);
            registerFile[code->m_srcIndex0] = Value(lnum);
            executeNextCode<BinaryUnsignedRightShift>(programCounter);
            NEXT_INSTRUCTION();
        }

        ThrowOperationOpcodeLbl:
        {
            ThrowOperation* code = (ThrowOperation*)currentCode;
            state.context()->throwException(state, registerFile[code->m_registerIndex]);
        }

        JumpComplexCaseOpcodeLbl:
        {
            JumpComplexCase* code = (JumpComplexCase*)currentCode;
            ASSERT(code->m_jumpPosition != SIZE_MAX);
            programCounter = jumpTo(codeBuffer, code->m_jumpPosition);
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
            NEXT_INSTRUCTION();
        }

        CreateObjectOpcodeLbl:
        {
            CreateObject* code = (CreateObject*)currentCode;
            registerFile[code->m_registerIndex] = new Object(state);
            executeNextCode<CreateObject>(programCounter);
            NEXT_INSTRUCTION();
        }

        EndOpcodeLbl:
        {
            if (!codeBlock->isGlobalScopeCodeBlock()) {
                *state.exeuctionResult() = Value();
            }
            return;
        }

        CallNativeFunctionOpcodeLbl:
        {
            CallNativeFunction* code = (CallNativeFunction*)currentCode;
            *state.exeuctionResult() = code->m_fn(state, env->record()->getThisBinding(), stackStorage, env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->isNewExpression());
            return;
        }
    }

    FillOpcodeTable:
    {
#define REGISTER_TABLE(opcode, pushCount, popCount) \
        registerOpcode(opcode##Opcode, &&opcode##OpcodeLbl);
        FOR_EACH_BYTECODE_OP(REGISTER_TABLE);

#undef REGISTER_TABLE
    }
    return;
}


Value ByteCodeInterpreter::loadByName(ExecutionState& state, LexicalEnvironment* env, const AtomicString& name)
{
    while (env) {
        EnvironmentRecord::GetBindingValueResult result = env->record()->getBindingValue(state, name);
        if (result.m_hasBindingValue) {
            return result.m_value;
        }
        env = env->outerEnvironment();
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::ReferenceError, name.string(), false, String::emptyString, errorMessage_IsNotDefined);
    RELEASE_ASSERT_NOT_REACHED();
}

Value ByteCodeInterpreter::plusSlowCase(ExecutionState& state, const Value& left, const Value& right)
{
    Value ret(Value::ForceUninitialized);
    Value lval(Value::ForceUninitialized);
    Value rval(Value::ForceUninitialized);

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.12.8
    // No hint is provided in the calls to ToPrimitive in steps 5 and 6.
    // All native ECMAScript objects except Date objects handle the absence of a hint as if the hint Number were given;
    // Date objects handle the absence of a hint as if the hint String were given.
    // Host objects may handle the absence of a hint in some other manner.
    if (left.isPointerValue() && left.asPointerValue()->isDateObject()) {
        lval = left.toPrimitive(state, Value::PreferString);
    } else {
        lval = left.toPrimitive(state);
    }

    if (right.isPointerValue() && right.asPointerValue()->isDateObject()) {
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

Value ByteCodeInterpreter::modOperation(ExecutionState& state, const Value& left, const Value& right)
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

inline bool ByteCodeInterpreter::abstractRelationalComparison(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
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

inline bool ByteCodeInterpreter::abstractRelationalComparisonOrEqual(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
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

bool ByteCodeInterpreter::abstractRelationalComparisonSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
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

bool ByteCodeInterpreter::abstractRelationalComparisonOrEqualSlowCase(ExecutionState& state, const Value& left, const Value& right, bool leftFirst)
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

}
