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

#ifndef BlockStatementNode_h
#define BlockStatementNode_h

#include "StatementNode.h"

namespace Escargot {

// A block statement, i.e., a sequence of statements surrounded by braces.
class BlockStatementNode : public StatementNode {
public:
    friend class ScriptParser;
    explicit BlockStatementNode(StatementContainer* body, LocalNamesVector&& localNames, size_t lexicalBlockIndex = 0, StatementContainer* argumentInitializers = nullptr)
        : StatementNode()
        , m_container(body)
        , m_argumentInitializers(argumentInitializers)
        , m_localNames(localNames)
        , m_lexicalBlockIndex(lexicalBlockIndex)
    {
    }

    explicit BlockStatementNode(StatementContainer* body, StatementContainer* argumentInitializers = nullptr, size_t lexicalBlockIndex = 0)
        : StatementNode()
        , m_container(body)
        , m_argumentInitializers(argumentInitializers)
        , m_lexicalBlockIndex(lexicalBlockIndex)
    {
    }

    virtual ~BlockStatementNode()
    {
    }
    virtual ASTNodeType type() { return ASTNodeType::BlockStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (m_argumentInitializers != nullptr) {
            m_argumentInitializers->generateStatementByteCode(codeBlock, context);
        }

        size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        context->m_lexicalBlockIndex = m_lexicalBlockIndex;
        size_t blockPos = codeBlock->pushLexicalBlock(context, m_localNames);
        bool hasNonLexicalStatementBefore = context->m_hasNonLexicalStatement;
        context->m_hasNonLexicalStatement = false;

        size_t start = codeBlock->currentCodeSize();
        m_container->generateStatementByteCode(codeBlock, context);

        codeBlock->finalizeLexicalBlock(context, blockPos, start);
        context->m_hasNonLexicalStatement = hasNonLexicalStatementBefore;
        context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
    }

    StatementNode* firstChild()
    {
        return m_container->firstChild();
    }

private:
    RefPtr<StatementContainer> m_container;
    RefPtr<StatementContainer> m_argumentInitializers;
    LocalNamesVector m_localNames;
    size_t m_lexicalBlockIndex;
};
}

#endif
