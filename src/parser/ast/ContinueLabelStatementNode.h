/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef ContinueLabelStatmentNode_h
#define ContinueLabelStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

class ContinueLabelStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    ContinueLabelStatementNode(String* label)
        : StatementNode()
    {
        m_label = label;
    }

    virtual ASTNodeType type() { return ASTNodeType::ContinueLabelStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
        context->pushLabeledContinuePositions(codeBlock->lastCodePosition<Jump>(), m_label);

        for (size_t i = 0; i < context->m_currentLabels->size(); i++) {
            if (m_label->equals(context->m_currentLabels->at(i).first)) {
                if (context->m_currentLabels->at(i).second) {
                    ByteCodeGenerateError err;
                    err.m_index = m_loc.index;
                    auto ret = codeBlock->computeNodeLOC(codeBlock->m_codeBlock->src(), codeBlock->m_codeBlock->sourceElementStart(), m_loc.index);
                    char message[512];
                    snprintf(message, sizeof(message), "Line %zu: Undefined label %s", ret.line, m_label->toUTF8StringData().data());
                    err.m_message = message;
                    throw err;
                }
            }
        }
    }

protected:
    String* m_label;
};
}

#endif
