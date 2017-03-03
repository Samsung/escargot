#include "Escargot.h"
#include "Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "parser/ast/Node.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/SandBox.h"
#include "util/Util.h"
#include "parser/ast/AST.h"

namespace Escargot {

Value Script::execute(ExecutionState& state, bool isEvalMode, bool needNewEnv, bool isOnGlobal)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, ((ProgramNode*)programNode)->scopeContext(), isEvalMode, isOnGlobal);

    delete m_topCodeBlock->m_cachedASTNode;
    m_topCodeBlock->m_cachedASTNode = nullptr;

    LexicalEnvironment* env;
    ExecutionContext* prevEc;
    {
        CodeBlock* globalCodeBlock = (needNewEnv) ? nullptr : m_topCodeBlock;
        LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, globalCodeBlock, state.context()->globalObject()), nullptr);
        if (UNLIKELY(needNewEnv)) {
            // NOTE: ES5 10.4.2.1 eval in strict mode
            prevEc = new ExecutionContext(state.context(), state.executionContext(), globalEnvironment, m_topCodeBlock->isStrict());
            EnvironmentRecord* record = new DeclarativeEnvironmentRecordNotIndexed(state, m_topCodeBlock->identifierInfos());
            env = new LexicalEnvironment(record, globalEnvironment);
        } else {
            env = globalEnvironment;
            prevEc = nullptr;
        }
    }

    ExecutionContext ec(state.context(), prevEc, env, m_topCodeBlock->isStrict());
    Value resultValue;
    Value thisValue(state.context()->globalObject());
    ExecutionState newState(&state, &ec, &resultValue);

    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = (Value*)alloca((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + 1 + literalStorageSize) * sizeof(Value));
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    stackStorage[0] = thisValue;
    Value* literalStorage = stackStorage + 1;
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }
    clearStack<512>();
    ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);

    return resultValue;
}

Script::ScriptSandboxExecuteResult Script::sandboxExecute(Context* ctx)
{
    ScriptSandboxExecuteResult result;
    SandBox sb(ctx);
    ExecutionState stateForInit(ctx);

    auto sandBoxResult = sb.run([&]() -> Value {
        return execute(stateForInit);
    });
    result.result = sandBoxResult.result;
    result.msgStr = sandBoxResult.msgStr;
    result.error.errorValue = sandBoxResult.error;
    if (!sandBoxResult.error.isEmpty()) {
        for (size_t i = 0; i < sandBoxResult.stackTraceData.size(); i++) {
            ScriptSandboxExecuteResult::Error::StackTrace t;
            t.fileName = sandBoxResult.stackTraceData[i].fileName;
            t.line = sandBoxResult.stackTraceData[i].loc.line;
            t.column = sandBoxResult.stackTraceData[i].loc.column;
            result.error.stackTrace.pushBack(t);
        }
    }
    return result;
}

// NOTE: eval by direct call
Value Script::executeLocal(ExecutionState& state, Value thisValue, bool isEvalMode, bool needNewRecord)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, ((ProgramNode*)programNode)->scopeContext(), isEvalMode);

    delete m_topCodeBlock->m_cachedASTNode;
    m_topCodeBlock->m_cachedASTNode = nullptr;

    EnvironmentRecord* record;
    if (UNLIKELY(needNewRecord)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        record = new DeclarativeEnvironmentRecordNotIndexed(state, m_topCodeBlock->identifierInfos());
    } else {
        record = state.executionContext()->lexicalEnvironment()->record();
    }
    const CodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        if (record->hasBinding(state, vec[i].m_name).m_index == SIZE_MAX) {
            record->createMutableBinding(state, vec[i].m_name, false);
        }
    }
    LexicalEnvironment* newEnvironment = new LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());

    ExecutionContext ec(state.context(), state.executionContext(), newEnvironment, m_topCodeBlock->isStrict());
    Value resultValue;
    ExecutionState newState(&state, &ec, &resultValue);

    size_t stackStorageSize = m_topCodeBlock->identifierOnStackCount();
    size_t literalStorageSize = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.size();
    Value* registerFile = ALLOCA((m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize + stackStorageSize + literalStorageSize) * sizeof(Value), Value, state);
    Value* stackStorage = registerFile + m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize;
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }
    Value* literalStorage = stackStorage + stackStorageSize;
    Value* src = m_topCodeBlock->byteCodeBlock()->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    stackStorage[m_topCodeBlock->thisSymbolIndex()] = thisValue;

    clearStack<512>();
    ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile);

    return resultValue;
}
}
