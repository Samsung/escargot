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

#ifndef LabeledStatementNode_h
#define LabeledStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class LabeledStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    LabeledStatementNode(Node* statementNode, String* label)
        : StatementNode()
        , m_statementNode(statementNode)
        , m_label(label)
    {
    }

    virtual ~LabeledStatementNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::LabeledStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t start = codeBlock->currentCodeSize();
        context->m_positionToContinue = start;
        m_statementNode->generateStatementByteCode(codeBlock, context);
        size_t end = codeBlock->currentCodeSize();
        context->consumeLabeledBreakPositions(codeBlock, end, m_label, context->tryCatchWithBlockStatementCount());
        context->consumeLabeledContinuePositions(codeBlock, context->m_positionToContinue, m_label, context->tryCatchWithBlockStatementCount());
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_statementNode->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_statementNode;
    String* m_label;
};
}

#endif
