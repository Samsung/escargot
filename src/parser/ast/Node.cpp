/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
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

void Node::generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
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
}
