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

#ifndef ExportAllDeclarationNode_h
#define ExportAllDeclarationNode_h

#include "Node.h"
#include "ExportDeclarationNode.h"

namespace Escargot {

class ExportAllDeclarationNode : public ExportDeclarationNode {
public:
    friend class ScriptParser;
    ExportAllDeclarationNode(RefPtr<Node> src)
        : m_src(src)
    {
        ASSERT(src->isLiteral());
    }

    virtual ASTNodeType type() override { return ASTNodeType::ExportAllDeclaration; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_src->iterateChildren(fn);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
    }

private:
    RefPtr<Node> m_src;
};
}

#endif
