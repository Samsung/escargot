/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#ifndef ThisExpressionNode_h
#define ThisExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ThisExpressionNode : public ExpressionNode {
public:
    ThisExpressionNode()
        : ExpressionNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ThisExpression; }
    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->pushRegister(REGULAR_REGISTER_LIMIT);
        return REGULAR_REGISTER_LIMIT;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (codeBlock->m_codeBlock->isClassConstructor()) {
            codeBlock->pushCode(LoadThisBinding(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT), context, this);
        }

        if (dstRegister != REGULAR_REGISTER_LIMIT) {
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT, dstRegister), context, this);
        }
    }

protected:
};
}

#endif
