#include "Escargot.h"
#include "ByteCode.h"
#include "ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/FunctionObject.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"

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
        size_t programCounter = (size_t)(&codeBuffer[0]);
        ByteCode* currentCode;

    #define NEXT_INSTRUCTION() \
        goto NextInstruction

        NextInstruction:
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
            Value v0 = registerFile[code->m_srcIndex0];
            Value v1 = registerFile[code->m_srcIndex1];
            // TODO
            registerFile[code->m_srcIndex0] = Value(v0.asNumber() + v1.asNumber());
            executeNextCode<StoreByName>(programCounter);
            NEXT_INSTRUCTION();
        }

        CallFunctionOpcodeLbl:
        {
            CallFunction* code = (CallFunction*)currentCode;
            const Value& callee = registerFile[code->m_registerIndex];
            if (!callee.isFunction()) {
                // TODO throw exception
                RELEASE_ASSERT_NOT_REACHED();
            }
            registerFile[code->m_registerIndex] = callee.asFunction()->call(state, callee, code->m_argumentCount, &registerFile[code->m_registerIndex + 1]);
            executeNextCode<CallFunction>(programCounter);
            NEXT_INSTRUCTION();
        }

        CallFunctionWithReceiverOpcodeLbl:
        {
            CallFunctionWithReceiver* code = (CallFunctionWithReceiver*)currentCode;
            const Value& callee = registerFile[code->m_registerIndex];
            if (!callee.isFunction()) {
                // TODO throw exception
                RELEASE_ASSERT_NOT_REACHED();
            }
            const Value& receiver = registerFile[code->m_registerIndex + 1];
            registerFile[code->m_registerIndex] = callee.asFunction()->call(state, receiver, code->m_argumentCount, &registerFile[code->m_registerIndex + 2]);
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

        EndOpcodeLbl:
        {
            if (!codeBlock->isGlobalScopeCodeBlock()) {
                *state.exeuctionResult() = Value();
            }
            return;
        }
    }

    FillOpcodeTable:
    {
#define REGISTER_TABLE(opcode, pushCount, popCount) \
        registerOpcode(opcode##Opcode, &&opcode##OpcodeLbl);
        FOR_EACH_BYTECODE_OP(REGISTER_TABLE);
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
    // TODO throw reference error
    RELEASE_ASSERT_NOT_REACHED();
}



}
