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

#ifndef VariableDeclarationNode_h
#define VariableDeclarationNode_h

#include "DeclarationNode.h"
#include "VariableDeclaratorNode.h"
#include "parser/Lexer.h"

namespace Escargot {

class VariableDeclarationNode : public DeclarationNode {
public:
    explicit VariableDeclarationNode(const NodeList& decl, EscargotLexer::KeywordKind kind)
        : DeclarationNode()
        , m_declarations(decl)
        , m_kind(kind)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::VariableDeclaration; }
    EscargotLexer::KeywordKind kind()
    {
        return m_kind;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        for (SentinelNode* declaration = m_declarations.begin(); declaration != m_declarations.end(); declaration = declaration->next()) {
            declaration->astNode()->generateStatementByteCode(codeBlock, context);
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex src, bool needToReferenceSelf) override
    {
        ASSERT(m_declarations.size() == 1);
        m_declarations.begin()->astNode()->asVariableDeclarator()->id()->generateStoreByteCode(codeBlock, context, src, true);
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        generateStatementByteCode(codeBlock, context);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* declaration = m_declarations.begin(); declaration != m_declarations.end(); declaration = declaration->next()) {
            declaration->astNode()->asVariableDeclarator()->id()->iterateChildren(fn);
            declaration->astNode()->asVariableDeclarator()->init()->iterateChildren(fn);
        }
    }

private:
    NodeList m_declarations; // declarations: [ VariableDeclarator ];
    EscargotLexer::KeywordKind m_kind;
};
} // namespace Escargot

#endif
