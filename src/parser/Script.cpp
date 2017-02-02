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

namespace Escargot {

Value Script::execute(ExecutionState& state, bool isEvalMode, bool needNewEnv, bool isOnGlobal)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, isEvalMode, isOnGlobal);

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
    ExecutionState newState(state.context(), &ec, &resultValue);

    Value* registerFile = (Value*)alloca(m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize * sizeof(Value));
    clearStack<512>();
    ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile, nullptr);

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
Value Script::executeLocal(ExecutionState& state, bool isEvalMode, bool needNewRecord)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    m_topCodeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_topCodeBlock, programNode, isEvalMode);

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
    ExecutionState newState(state.context(), &ec, &resultValue);

    size_t stackStorageSize = m_topCodeBlock->identifierOnStackCount();
    Value* stackStorage = ALLOCA(stackStorageSize * sizeof(Value), Value, state);
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }

    Value* registerFile = (Value*)alloca(m_topCodeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize * sizeof(Value));
    clearStack<512>();
    ByteCodeInterpreter::interpret(newState, m_topCodeBlock->byteCodeBlock(), 0, registerFile, stackStorage);

    return resultValue;
}
}
