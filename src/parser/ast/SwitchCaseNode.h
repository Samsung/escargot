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

#ifndef SwitchCaseNode_h
#define SwitchCaseNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class SwitchCaseNode : public StatementNode {
public:
    SwitchCaseNode(Node* test, StatementContainer* consequent)
        : StatementNode()
        , m_test(test)
        , m_consequent(consequent)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::SwitchCase; }
    bool isDefaultNode()
    {
        return !m_test;
    }

    Node* test()
    {
        ASSERT(!!m_test);
        return m_test;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        m_consequent->generateStatementByteCode(codeBlock, context);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_test) {
            m_test->iterateChildren(fn);
        }
        m_consequent->iterateChildren(fn);
    }

private:
    Node* m_test;
    StatementContainer* m_consequent;
};
}

#endif
