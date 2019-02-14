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

#ifndef ContinueLabelStatmentNode_h
#define ContinueLabelStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

class ContinueLabelStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    explicit ContinueLabelStatementNode(String* label)
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

private:
    String* m_label;
};
}

#endif
