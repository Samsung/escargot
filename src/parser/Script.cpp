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

Script::ScriptExecuteResult Script::execute(Context* ctx)
{
    ScriptExecuteResult result;

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

    SandBox sb(ctx);
    auto sandBoxResult = sb.run([&]() -> Value {
        ByteCodeInterpreter::interpret(state, m_topCodeBlock);
        return resultValue;
    });
    result.result = sandBoxResult.result;
    result.error.errorValue = sandBoxResult.error;
    if (!sandBoxResult.error.isEmpty()) {
        for (size_t i = 0; i < sandBoxResult.stackTraceData.size(); i++) {
            ScriptExecuteResult::Error::StackTrace t;
            t.fileName = sandBoxResult.stackTraceData[i].fileName;
            t.line = sandBoxResult.stackTraceData[i].loc.line;
            t.column = sandBoxResult.stackTraceData[i].loc.column;
            result.error.stackTrace.pushBack(t);
        }
    }
    return result;
}
}
