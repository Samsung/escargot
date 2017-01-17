/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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

#ifndef LabeledStatementNode_h
#define LabeledStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class LabeledStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    LabeledStatementNode(StatementNode* statementNode, String* label)
        : StatementNode()
    {
        m_statementNode = statementNode;
        m_label = label;
    }

    virtual ~LabeledStatementNode()
    {
        delete m_statementNode;
    }

    virtual ASTNodeType type() { return ASTNodeType::LabeledStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t start = codeBlock->currentCodeSize();
        context->m_positionToContinue = start;
        m_statementNode->generateStatementByteCode(codeBlock, context);
        size_t end = codeBlock->currentCodeSize();
        context->consumeLabeledBreakPositions(codeBlock, end, m_label);
        context->consumeLabeledContinuePositions(codeBlock, context->m_positionToContinue, m_label);
    }

protected:
    StatementNode* m_statementNode;
    String* m_label;
};
}

#endif
