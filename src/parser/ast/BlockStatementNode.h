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

#ifndef BlockStatementNode_h
#define BlockStatementNode_h

#include "StatementNode.h"

namespace Escargot {

// A block statement, i.e., a sequence of statements surrounded by braces.
class BlockStatementNode : public StatementNode {
public:
    explicit BlockStatementNode(StatementContainer* body, size_t lexicalBlockIndex = 0)
        : StatementNode()
        , m_container(body)
        , m_lexicalBlockIndex(lexicalBlockIndex)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::BlockStatement; }
    LexicalBlockIndex lexicalBlockIndex() { return m_lexicalBlockIndex; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
        if (m_lexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_lexicalBlockIndex;
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_lexicalBlockIndex);
            blockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        size_t start = codeBlock->currentCodeSize();
        m_container->generateStatementByteCode(codeBlock, context);

        if (m_lexicalBlockIndex != LEXICAL_BLOCK_INDEX_MAX) {
            codeBlock->finalizeLexicalBlock(context, blockContext);
            context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
        }
    }

    StatementContainer* container()
    {
        return m_container;
    }

    StatementNode* firstChild()
    {
        return m_container->firstChild();
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_container->iterateChildren(fn);
    }

#ifdef ESCARGOT_DEBUGGER
    virtual bool isEmptyStatement(void)
    {
        StatementNode* node = firstChild();
        while (node) {
            if (!node->isEmptyStatement()) {
                return false;
            }
            node = node->nextSibling();
        }

        return true;
    }
#endif /* ESCARGOT_DEBUGGER */

private:
    StatementContainer* m_container;
    LexicalBlockIndex m_lexicalBlockIndex;
};
} // namespace Escargot

#endif
