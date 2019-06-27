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

#ifndef PropertyNode_h
#define PropertyNode_h

#include "Node.h"
#include "StatementNode.h"

namespace Escargot {

class PropertyNode : public Node {
public:
    enum Kind {
        Init,
        Get,
        Set
    };

    PropertyNode(Node* key, Node* value, Kind kind, bool computed)
        : Node()
        , m_kind(kind)
        , m_computed(computed)
        , m_key(key)
        , m_value(value)
    {
    }

    virtual ~PropertyNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Property; }
    Node* key()
    {
        return m_key;
    }

    Node* value()
    {
        return m_value;
    }

    Kind kind()
    {
        return m_kind;
    }

    bool computed()
    {
        return m_computed;
    }

    bool isValidPropertyNode()
    {
        return m_key->isIdentifier() || m_key->isLiteral();
    }

private:
    Kind m_kind; // kind: "init" | "get" | "set";
    bool m_computed : 1;
    Node* m_key; // key: Literal | Identifier;
    Node* m_value; // value: Expression;
};
}

#endif
