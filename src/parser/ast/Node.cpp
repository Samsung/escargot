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

void* ASTScopeContext::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ASTScopeContext)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_parentContext));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_associateNode));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_names));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_usingNames));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_parameters));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_childScopes));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ASTScopeContext, m_numeralLiteralData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ASTScopeContext));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}
}
