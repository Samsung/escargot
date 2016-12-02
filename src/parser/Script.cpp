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

namespace Escargot {

Script::ScriptExecuteResult Script::execute(Context* ctx)
{
    ScriptExecuteResult result;

    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    g.generateByteCode(ctx, m_topCodeBlock, programNode);

    ExecutionState stateForInit(ctx);
    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(stateForInit, m_topCodeBlock, ctx->globalObject()), nullptr);

    ExecutionContext ec(ctx, globalEnvironment, m_topCodeBlock->isStrict());
    Value resultValue;
    ExecutionState state(ctx, &ec, &resultValue);
    try {
        ByteCodeInterpreter::interpret(state, m_topCodeBlock);
        result.result = resultValue;
        return result;
    } catch(ErrorObject* err) {
        result.result = Value(err).toString(state);
        return result;
    }
}

}
