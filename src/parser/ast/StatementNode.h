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

#ifndef StatmentNode_h
#define StatmentNode_h

#include "Node.h"

namespace Escargot {

class StatementNode : public Node {
    friend class StatementContainer;

public:
    StatementNode()
        : Node()
    {
    }

    virtual bool isStatement()
    {
        return true;
    }

    StatementNode* nextSibling()
    {
        return m_nextSibling.get();
    }

private:
    RefPtr<StatementNode> m_nextSibling;
};

class StatementContainer : public RefCounted<StatementContainer> {
public:
    static RefPtr<StatementContainer> create()
    {
        return adoptRef(new StatementContainer());
    }

    ~StatementContainer()
    {
        RefPtr<StatementNode> c = m_firstChild.release();
        if (!c) {
            return;
        }

        do {
            RefPtr<StatementNode> next = c->m_nextSibling.release();
            c.release();
            c = next;
        } while (c);
    }

    void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        StatementNode* nd = firstChild();
        while (nd) {
            nd->generateStatementByteCode(codeBlock, context);
            nd = nd->nextSibling();
        }
    }

    void iterateChildren(const std::function<void(Node* node)>& fn)
    {
        StatementNode* nd = firstChild();
        while (nd) {
            nd->iterateChildren(fn);
            nd = nd->nextSibling();
        }
    }

    StatementNode* appendChild(RefPtr<Node> c)
    {
        return appendChild(c.get());
    }

    StatementNode* appendChild(RefPtr<Node> c, RefPtr<Node> referNode)
    {
        return appendChild(c.get(), referNode.get());
    }

    StatementNode* appendChild(Node* c, Node* referNode)
    {
        ASSERT(c->isStatement());
        StatementNode* child = c->asStatement();
        if (referNode == nullptr) {
            appendChild(child);
        } else {
            ASSERT(referNode->isStatement());
            referNode->asStatement()->m_nextSibling = child;
        }
        return child;
    }

    StatementNode* appendChild(Node* c)
    {
        ASSERT(c->isStatement());
        ASSERT(c->asStatement()->nextSibling() == nullptr);
        StatementNode* child = c->asStatement();
        if (m_firstChild == nullptr) {
            m_firstChild = child;
        } else {
            StatementNode* tail = firstChild();
            while (tail->m_nextSibling != nullptr) {
                tail = tail->m_nextSibling.get();
            }
            tail->m_nextSibling = child;
        }
        return child;
    }

    StatementNode* firstChild()
    {
        return m_firstChild.get();
    }

private:
    RefPtr<StatementNode> m_firstChild;
};
}

#endif
