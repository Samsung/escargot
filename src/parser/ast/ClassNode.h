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

#ifndef ClassNode_h
#define ClassNode_h

#include "Node.h"
#include "ClassBodyNode.h"

namespace Escargot {

class ClassNode {
public:
    ClassNode(RefPtr<IdentifierNode>& id, RefPtr<Node> superClass, RefPtr<ClassBodyNode> classBody, LexicalBlockIndex classBodyLexicalBlockIndex)
        : m_classBodyLexicalBlockIndex(classBodyLexicalBlockIndex)
        , m_id(id)
        , m_superClass(superClass)
        , m_classBody(classBody)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Class; }
    inline const RefPtr<IdentifierNode>& id() { return m_id; }
    inline const RefPtr<Node> superClass() { return m_superClass; }
    inline const RefPtr<ClassBodyNode> classBody() { return m_classBody; }
    inline LexicalBlockIndex classBodyLexicalBlockIndex() { return m_classBodyLexicalBlockIndex; }
private:
    LexicalBlockIndex m_classBodyLexicalBlockIndex;
    RefPtr<IdentifierNode> m_id; // Id
    RefPtr<Node> m_superClass;
    RefPtr<ClassBodyNode> m_classBody;
};
}

#endif
