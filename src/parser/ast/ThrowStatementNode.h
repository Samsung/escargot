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

#ifndef ThrowStatementNode_h
#define ThrowStatementNode_h

#include "StatementNode.h"

namespace Escargot {

// interface ThrowStatement <: Statement {
class ThrowStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    explicit ThrowStatementNode(Node* argument)
        : StatementNode()
        , m_argument(argument)
    {
    }
    virtual ~ThrowStatementNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ThrowStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ASSERT(codeBlock != nullptr);
        ASSERT(context != nullptr);

        if (codeBlock->m_codeBlock->isGenerator() == true) {
            codeBlock->pushCode(GeneratorComplete(ByteCodeLOC(SIZE_MAX), true), context, this);
        }

        context->getRegister();
        auto r = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, r);
        codeBlock->pushCode(ThrowOperation(ByteCodeLOC(m_loc.index), r), context, this);
        context->giveUpRegister();
        context->giveUpRegister();
    }

private:
    RefPtr<Node> m_argument;
};
}

#endif
