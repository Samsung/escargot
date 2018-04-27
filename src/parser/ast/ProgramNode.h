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

#ifndef ProgramNode_h
#define ProgramNode_h

#include "Node.h"
#include "StatementNode.h"
#include "parser/ScriptParser.h"

namespace Escargot {

class ProgramNode : public Node {
public:
    friend class ScriptParser;
    ProgramNode(StatementNodeVector&& body, ASTScopeContext* scopeContext)
        : Node()
    {
        m_body = body;
        m_scopeContext = scopeContext;
        m_scopeContext->m_nodeType = type();
    }

    virtual ~ProgramNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Program; }
    ASTScopeContext* scopeContext() { return m_scopeContext; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t len = m_body.size();

        for (size_t i = 0; i < len; i++) {
            m_body[i]->generateStatementByteCode(codeBlock, context);
        }
        codeBlock->pushCode(End(ByteCodeLOC(SIZE_MAX)), context, this);
    }

protected:
    StatementNodeVector m_body; // body: [ Statement ];
    ASTScopeContext* m_scopeContext;
};
}

#endif
