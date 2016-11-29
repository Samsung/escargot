#include "Escargot.h"
#include "Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "parser/ast/Node.h"
#include "runtime/Context.h"

namespace Escargot {

Script::ScriptExecuteResult Script::execute(Context* ctx)
{
    ScriptExecuteResult result;

    Node* programNode = m_topCodeBlock->cachedASTNode();
    ASSERT(programNode && programNode->type() == ASTNodeType::Program);

    ByteCodeGenerator g;
    g.generateByteCode(ctx, m_topCodeBlock, programNode);

    ExecutionContext ec(ctx, ctx->globalEnvironment());
    Value resultValue;
    ExecutionState state(ctx, &ec, &resultValue);
    ByteCodeIntrepreter::interpret(state, m_topCodeBlock);
    result.result = resultValue;
    return result;
}

}
