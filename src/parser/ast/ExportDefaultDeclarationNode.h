/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ExportDefaultDeclarationNode_h
#define ExportDefaultDeclarationNode_h

#include "Node.h"
#include "StatementNode.h"
#include "ExportDeclarationNode.h"

namespace Escargot {

class ExportDefaultDeclarationNode : public ExportDeclarationNode {
public:
    ExportDefaultDeclarationNode(Node* declaration, AtomicString exportName, AtomicString localName)
        : m_declaration(declaration)
        , m_exportName(exportName)
        , m_localName(localName)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ExportDefaultDeclaration; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_declaration->iterateChildren(fn);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t r = context->getRegister();
        m_declaration->generateExpressionByteCode(codeBlock, context, r);

        IdentifierNode* id = new (alloca(sizeof(IdentifierNode))) IdentifierNode(m_localName);
        id->m_loc = m_loc;
        context->m_isLexicallyDeclaredBindingInitialization = true;
        id->generateStoreByteCode(codeBlock, context, r, true);

        context->giveUpRegister();
    }

private:
    Node* m_declaration;
    AtomicString m_exportName;
    AtomicString m_localName;
};
}

#endif
