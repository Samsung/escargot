#include "Escargot.h"
#include "Node.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"

namespace Escargot {

ByteCodeRegisterIndex Node::getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
{
    return context->getRegister();
}

void Node::generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
{
#ifndef NDEBUG
    size_t before = context->m_registerStack->size();
#endif
    generateExpressionByteCode(codeBlock, context, getRegister(codeBlock, context));
    context->giveUpRegister();
    ASSERT(context->m_registerStack->size() == before);
}
}
