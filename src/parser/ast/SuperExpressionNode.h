/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version
. *
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

#ifndef SuperExpressionNode_h
#define SuperExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class SuperExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;

    SuperExpressionNode(bool isCall)
        : ExpressionNode()
        , m_isCall(isCall)
    {
    }

    bool isCall()
    {
        return m_isCall;
    }

    virtual ASTNodeType type() override { return ASTNodeType::SuperExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        codeBlock->pushCode(SuperReference(ByteCodeLOC(m_loc.index), dstIndex, m_isCall), context, this);
    }

private:
    bool m_isCall;
};
}

#endif
