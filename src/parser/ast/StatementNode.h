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

    virtual bool isStatementNode()
    {
        return true;
    }

    StatementNode* nextSilbing()
    {
        return m_nextSilbing.get();
    }

protected:
    RefPtr<StatementNode> m_nextSilbing;
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
            RefPtr<StatementNode> next = c->m_nextSilbing.release();
            c.release();
            c = next;
        } while (c);
    }

    void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        StatementNode* nd = m_firstChild.get();
        while (nd) {
            nd->generateStatementByteCode(codeBlock, context);
            nd = nd->nextSilbing();
        }
    }

    StatementNode* appendChild(RefPtr<StatementNode> c)
    {
        return appendChild(c.get());
    }

    StatementNode* appendChild(RefPtr<StatementNode> c, RefPtr<StatementNode> referNode)
    {
        return appendChild(c.get(), referNode.get());
    }

    StatementNode* appendChild(StatementNode* c, StatementNode* referNode)
    {
        if (referNode == nullptr) {
            appendChild(c);
        } else {
            referNode->m_nextSilbing = c;
        }
        return c;
    }

    StatementNode* appendChild(StatementNode* c)
    {
        ASSERT(c->nextSilbing() == nullptr);
        if (m_firstChild == nullptr) {
            m_firstChild = c;
        } else {
            StatementNode* tail = m_firstChild.get();
            while (tail->m_nextSilbing != nullptr) {
                tail = tail->m_nextSilbing.get();
            }
            tail->m_nextSilbing = c;
        }
        return c;
    };

    StatementNode* firstChild()
    {
        return m_firstChild.get();
    }

protected:
    RefPtr<StatementNode> m_firstChild;
};
}

#endif
