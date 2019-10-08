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

#ifndef ImportSpecifierNode_h
#define ImportSpecifierNode_h

#include "Node.h"

namespace Escargot {

class ImportSpecifierNode : public Node {
public:
    friend class ScriptParser;
    ImportSpecifierNode(RefPtr<Node> local, RefPtr<Node> imported)
        : m_local(local)
        , m_imported(imported)
    {
        ASSERT(local->isIdentifier() && imported->asIdentifier());
    }

    virtual ASTNodeType type() override { return ASTNodeType::ImportSpecifier; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_local->iterateChildren(fn);
        m_imported->iterateChildren(fn);
    }

    IdentifierNode* local()
    {
        return m_local->asIdentifier();
    }

    IdentifierNode* imported()
    {
        return m_imported->asIdentifier();
    }

private:
    RefPtr<Node> m_local;
    RefPtr<Node> m_imported;
};
}

#endif
