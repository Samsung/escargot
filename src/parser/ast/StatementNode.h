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

#ifndef StatmentNode_h
#define StatmentNode_h

#include "Node.h"

namespace Escargot {

class StatementNode : public Node {
    friend class StatementContainer;

public:
    StatementNode()
        : Node()
        , m_nextSibling(nullptr)
    {
    }

    virtual bool isStatement()
    {
        return true;
    }

    StatementNode* nextSibling()
    {
        return m_nextSibling;
    }

#ifdef ESCARGOT_DEBUGGER
    virtual bool isEmptyStatement(void)
    {
        return false;
    }

    inline void insertBreakpoint(ByteCodeGenerateContext* context)
    {
        context->insertBreakpoint(m_loc.index, this);
    }

    void insertEmptyStatementBreakpoint(ByteCodeGenerateContext* context, Node* node)
    {
        // The first statement of a loop always stops (even if it is an empty statement).
        // Otherwise the debugger may not be able to track the iterations of a loop.

        if (node->type() == ASTNodeType::EmptyStatement || node->type() == ASTNodeType::BlockStatement) {
            StatementNode* statementNode = reinterpret_cast<StatementNode*>(node);
            if (statementNode->isEmptyStatement()) {
                context->insertBreakpoint(statementNode->m_loc.index, node);
            }
        }
    }
#endif /* ESCARGOT_DEBUGGER */

private:
    StatementNode* m_nextSibling;
};

class StatementContainer {
public:
    static StatementContainer* create(ASTAllocator& allocator)
    {
        return new (allocator) StatementContainer();
    }

    StatementContainer()
        : m_firstChild(nullptr)
    {
    }

    ~StatementContainer()
    {
    }

    // StatementContainer is allocated by ASTAllocator
    inline void* operator new(size_t size, ASTAllocator& allocator)
    {
        return allocator.allocate(size);
    }

    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;

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

    StatementNode* appendChild(Node* c, Node* referNode)
    {
        ASSERT(c->isStatement());
        StatementNode* child = (StatementNode*)c;
        if (referNode == nullptr) {
            appendChild(child);
        } else {
            ASSERT(referNode->isStatement());
            ((StatementNode*)referNode)->m_nextSibling = child;
        }
        return child;
    }

    StatementNode* appendChild(Node* c)
    {
        ASSERT(c->isStatement());
        ASSERT(((StatementNode*)c)->nextSibling() == nullptr);
        StatementNode* child = (StatementNode*)c;
        if (m_firstChild == nullptr) {
            m_firstChild = child;
        } else {
            StatementNode* tail = firstChild();
            while (tail->m_nextSibling != nullptr) {
                tail = tail->m_nextSibling;
            }
            tail->m_nextSibling = child;
        }
        return child;
    }

    StatementNode* firstChild()
    {
        return m_firstChild;
    }

private:
    StatementNode* m_firstChild;
};
} // namespace Escargot

#endif
