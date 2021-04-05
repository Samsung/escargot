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

#ifndef BreakLabelStatmentNode_h
#define BreakLabelStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

class BreakLabelStatementNode : public StatementNode {
public:
    explicit BreakLabelStatementNode(String* label)
        : StatementNode()
        , m_label(label)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::BreakLabelStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
        context->pushLabelledBreakPositions(codeBlock->lastCodePosition<Jump>(), m_label);
    }

private:
    String* m_label;
};
} // namespace Escargot

#endif
