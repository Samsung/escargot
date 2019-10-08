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

#ifndef ExportSpecifierNode_h
#define ExportSpecifierNode_h

#include "Node.h"

namespace Escargot {

class ExportSpecifierNode : public Node {
public:
    friend class ScriptParser;
    ExportSpecifierNode(RefPtr<Node> local, RefPtr<Node> exported)
        : m_local(local)
        , m_exported(exported)
    {
        ASSERT(local->isIdentifier() && exported->isIdentifier());
    }

    virtual ASTNodeType type() override { return ASTNodeType::ExportSpecifier; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_local->iterateChildren(fn);
        m_exported->iterateChildren(fn);
    }

    bool isLocalAndExportedDiffer() // seen "as" keyword
    {
        return m_local != m_exported;
    }

    IdentifierNode* local()
    {
        return m_local->asIdentifier();
    }

    IdentifierNode* exported()
    {
        return m_exported->asIdentifier();
    }

private:
    RefPtr<Node> m_local;
    RefPtr<Node> m_exported;
};
}

#endif
