/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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

#ifndef BinaryExpressionNode_h
#define BinaryExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class BinaryExpressionNode : public ExpressionNode {
public:
    BinaryExpressionNode(Node* left, Node* right)
        : m_left(left)
        , m_right(right)
    {
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) final
    {
        ByteCodeGenerateContext::checkStack();
        m_left->iterateChildrenIdentifier(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) final
    {
        ByteCodeGenerateContext::checkStack();
        fn(this);

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
    }

protected:
    Node* m_left;
    Node* m_right;
};
} // namespace Escargot

#endif
