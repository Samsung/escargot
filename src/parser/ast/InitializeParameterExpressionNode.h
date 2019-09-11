/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef InitializeParameterExpressionNode_h
#define InitializeParameterExpressionNode_h

#include "IdentifierNode.h"


namespace Escargot {

class InitializeParameterExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;

    InitializeParameterExpressionNode(Node* left, size_t index)
        : ExpressionNode()
        , m_left(left)
        , m_paramIndex(index)
    {
    }

    virtual ~InitializeParameterExpressionNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::InitializeParameterExpression; }
    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        // ignore LexicalBlockIndex during parameter initialization
        auto oldLexicalBlockIndex = context->m_lexicalBlockIndex;
        context->m_lexicalBlockIndex = LEXICAL_BLOCK_INDEX_MAX;

        if (m_left->isIdentifier()) {
            auto r = m_left->asIdentifier()->isAllocatedOnStack(context);
            if (std::get<0>(r)) {
                codeBlock->pushCode(GetParameter(ByteCodeLOC(m_loc.index), std::get<1>(r), m_paramIndex), context, this);
            } else {
                size_t rightRegister = context->getRegister();
                m_left->generateResolveAddressByteCode(codeBlock, context);
                codeBlock->pushCode(GetParameter(ByteCodeLOC(m_loc.index), rightRegister, m_paramIndex), context, this);
                m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
                context->giveUpRegister();
            }
        } else {
            // pattern or default parameter cases
            size_t rightRegister = context->getRegister();
            m_left->generateResolveAddressByteCode(codeBlock, context);
            codeBlock->pushCode(GetParameter(ByteCodeLOC(m_loc.index), rightRegister, m_paramIndex), context, this);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        }

        context->m_lexicalBlockIndex = oldLexicalBlockIndex;
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_left->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_left;
    size_t m_paramIndex; // parameter index
};
}

#endif
