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
    ExportSpecifierNode(RefPtr<IdentifierNode> local, RefPtr<IdentifierNode> exported)
        : m_local(local)
        , m_exported(exported)
    {
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

    RefPtr<IdentifierNode> local()
    {
        return m_local;
    }

    RefPtr<IdentifierNode> exported()
    {
        return m_exported;
    }

private:
    RefPtr<IdentifierNode> m_local;
    RefPtr<IdentifierNode> m_exported;
};

typedef std::vector<RefPtr<ExportSpecifierNode>> ExportSpecifierNodeVector;
}

#endif
