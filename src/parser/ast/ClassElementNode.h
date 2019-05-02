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

#ifndef ClassElementNode_h
#define ClassElementNode_h

#include "Node.h"
#include "ClassElementNode.h"

namespace Escargot {

class ClassElementNode : public Node {
public:
    enum Kind {
        None,
        Get,
        Set,
        Method,
    };

    friend class ScriptParser;
    ClassElementNode(Node* key, Node* value, Kind kind, bool isComputed, bool isStatic)
        : Node()
        , m_key(key)
        , m_value(value)
        , m_kind(kind)
        , m_isComputed(isComputed)
        , m_isStatic(isStatic)
    {
    }

    Node* key()
    {
        return m_key.get();
    }

    Node* value()
    {
        return m_value.get();
    }

    Kind kind()
    {
        return m_kind;
    }

    bool isComputed()
    {
        return m_isComputed;
    }

    bool isStatic()
    {
        return m_isStatic;
    }

    virtual ASTNodeType type() { return ASTNodeType::ClassBody; }
private:
    RefPtr<Node> m_key; // key: Literal | Identifier;
    RefPtr<Node> m_value; // value: Expression;
    Kind m_kind;
    bool m_isComputed : 1;
    bool m_isStatic : 1;
};
}

#endif
