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

#ifndef ExportNamedDeclarationNode_h
#define ExportNamedDeclarationNode_h

#include "Node.h"
#include "ExportSpecifierNode.h"
#include "ExportDeclarationNode.h"

namespace Escargot {

class ExportNamedDeclarationNode : public ExportDeclarationNode {
public:
    ExportNamedDeclarationNode(Node* declaration, NodeList& specifiers, Node* source)
        : m_declaration(declaration)
        , m_specifiers(specifiers)
        , m_source(source)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ExportNamedDeclaration; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_declaration) {
            m_declaration->iterateChildren(fn);
        }

        for (SentinelNode* specifier = m_specifiers.begin(); specifier != m_specifiers.end(); specifier = specifier->next()) {
            specifier->astNode()->iterateChildren(fn);
        }

        if (m_source) {
            m_source->iterateChildren(fn);
        }
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (m_declaration) {
            m_declaration->generateStatementByteCode(codeBlock, context);
        }
    }


private:
    Node* m_declaration;
    NodeList m_specifiers;
    Node* m_source;
};
}

#endif
