/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "Node.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "runtime/ErrorObject.h"

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

void Node::generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf, bool isInitializeBinding)
{
    generateExpressionByteCode(codeBlock, context, context->getRegister());
    context->giveUpRegister();
    codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::ReferenceError, "Invalid assignment left-hand side"), context, this);
}

void Node::generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
{
    context->getRegister();
    codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::ReferenceError, "Invalid assignment left-hand side"), context, this);
    return;
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
