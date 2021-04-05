/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef LabelledStatementNode_h
#define LabelledStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class LabelledStatementNode : public StatementNode {
public:
    LabelledStatementNode(Node* statementNode, String* label)
        : StatementNode()
        , m_statementNode(statementNode)
        , m_label(label)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::LabelledStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t start = codeBlock->currentCodeSize();
        context->m_positionToContinue = start;
        m_statementNode->generateStatementByteCode(codeBlock, context);
        size_t end = codeBlock->currentCodeSize();
        context->consumeLabelledBreakPositions(codeBlock, end, m_label, context->tryCatchWithBlockStatementCount());
        context->consumeLabelledContinuePositions(codeBlock, context->m_positionToContinue, m_label, context->tryCatchWithBlockStatementCount());
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_statementNode->iterateChildren(fn);
    }

private:
    Node* m_statementNode;
    String* m_label;
};
} // namespace Escargot

#endif
