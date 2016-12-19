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

namespace Escargot {

Value Script::execute(Context* ctx)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    g.generateByteCode(ctx, m_topCodeBlock, programNode);

    m_topCodeBlock->m_cachedASTNode = nullptr;

    LexicalEnvironment* globalEnvironment;
    {
        ExecutionState stateForInit(ctx);
        globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(stateForInit, m_topCodeBlock, ctx->globalObject()), nullptr);
    }

    ExecutionContext ec(ctx, nullptr, globalEnvironment, m_topCodeBlock->isStrict());
    Value resultValue;
    ExecutionState state(ctx, &ec, &resultValue);

    ByteCodeInterpreter::interpret(state, m_topCodeBlock, 0, nullptr);

    return resultValue;
}

Script::ScriptSandboxExecuteResult Script::sandboxExecute(Context* ctx)
{
    ScriptSandboxExecuteResult result;
    SandBox sb(ctx);
    auto sandBoxResult = sb.run([&]() -> Value {
        return execute(ctx);
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

Value Script::executeLocal(ExecutionState& state)
{
    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    g.generateByteCode(state.context(), m_topCodeBlock, programNode);

    m_topCodeBlock->m_cachedASTNode = nullptr;

    EnvironmentRecord* oldRecord = state.executionContext()->lexicalEnvironment()->record();
    const CodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        if (oldRecord->hasBinding(state, vec[i].m_name).m_index == SIZE_MAX) {
            oldRecord->createMutableBinding(state, vec[i].m_name, false);
        }
    }
    LexicalEnvironment* newEnvironment = new LexicalEnvironment(oldRecord, state.executionContext()->lexicalEnvironment());

    ExecutionContext ec(state.context(), state.executionContext(), newEnvironment, m_topCodeBlock->isStrict());
    Value resultValue;
    ExecutionState newState(state.context(), &ec, &resultValue);

    size_t stackStorageSize = m_topCodeBlock->identifierOnStackCount();
    Value* stackStorage = ALLOCA(stackStorageSize * sizeof(Value), Value, state);
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }

    ByteCodeInterpreter::interpret(newState, m_topCodeBlock, 0, stackStorage);

    return resultValue;
}
}
