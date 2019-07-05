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

class ProgramNode : public StatementNode {
public:
    friend class ScriptParser;
    ProgramNode(StatementContainer* body, ASTScopeContext* scopeContext, LocalNamesVector&& localNames)
        : StatementNode()
        , m_container(body)
        , m_scopeContext(scopeContext)
        , m_localNames(localNames)
    {
        m_scopeContext->m_nodeType = type();
    }

    virtual ~ProgramNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Program; }
    ASTScopeContext* scopeContext() { return m_scopeContext; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t blockPos = codeBlock->pushLexicalBlock(context, m_localNames);

        size_t start = codeBlock->currentCodeSize();
        m_container->generateStatementByteCode(codeBlock, context);

        codeBlock->finalizeLexicalBlock(context, blockPos, start);

        codeBlock->pushCode(End(ByteCodeLOC(SIZE_MAX)), context, this);
    }

private:
    RefPtr<StatementContainer> m_container;
    ASTScopeContext* m_scopeContext;
    LocalNamesVector m_localNames;
};
}

#endif
