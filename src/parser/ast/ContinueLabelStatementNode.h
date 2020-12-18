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

#ifndef ContinueLabelStatmentNode_h
#define ContinueLabelStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

class ContinueLabelStatementNode : public StatementNode {
public:
    explicit ContinueLabelStatementNode(String* label)
        : StatementNode()
        , m_label(label)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ContinueLabelStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
        context->pushLabeledContinuePositions(codeBlock->lastCodePosition<Jump>(), m_label);
    }

private:
    String* m_label;
};
} // namespace Escargot

#endif
