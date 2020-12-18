/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ImportDeclarationNode_h
#define ImportDeclarationNode_h

#include "StatementNode.h"
#include "ImportSpecifierNode.h"

namespace Escargot {

class ImportDeclarationNode : public StatementNode {
public:
    ImportDeclarationNode(const NodeList& specifiers, Node* src)
        : m_specifiers(specifiers)
        , m_src(src)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ImportDeclaration; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* specifier = m_specifiers.begin(); specifier != m_specifiers.end(); specifier = specifier->next()) {
            specifier->astNode()->iterateChildren(fn);
        }
        m_src->iterateChildren(fn);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
    }

private:
    NodeList m_specifiers;
    Node* m_src;
};
} // namespace Escargot

#endif
