/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef ClassNode_h
#define ClassNode_h

#include "Node.h"
#include "ClassBodyNode.h"

namespace Escargot {

class ClassNode {
public:
    ClassNode()
        : m_classBodyLexicalBlockIndex(0)
        , m_id()
        , m_superClass()
        , m_classBody()
        , m_classSrc()
    {
    }

    ClassNode(Node* id, Node* superClass, Node* classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
        : m_classBodyLexicalBlockIndex(classBodyLexicalBlockIndex)
        , m_id(id)
        , m_superClass(superClass)
        , m_classBody(classBody)
        , m_classSrc(classSrc)
    {
    }

    inline Node* id() const { return m_id; }
    inline Node* superClass() const { return m_superClass; }
    inline ClassBodyNode* classBody() const { return (ClassBodyNode*)m_classBody; }
    inline LexicalBlockIndex classBodyLexicalBlockIndex() const { return m_classBodyLexicalBlockIndex; }
    inline const StringView& classSrc() const { return m_classSrc; }
private:
    LexicalBlockIndex m_classBodyLexicalBlockIndex;
    Node* m_id; // Id
    Node* m_superClass;
    Node* m_classBody;
    StringView m_classSrc;
};
}

#endif
