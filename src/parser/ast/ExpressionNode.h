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

#ifndef ExpressionNode_h
#define ExpressionNode_h

#include "Node.h"


namespace Escargot {

// Any expression node. Since the left-hand side of an assignment may be any expression in general, an expression can also be a pattern.
// interface Expression <: Node, Pattern { }
class ExpressionNode : public Node {
public:
    ExpressionNode()
        : Node()
    {
    }

    virtual bool isExpression() override
    {
        return true;
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

    // for binary expressions
    static bool canUseDirectRegister(ByteCodeGenerateContext* context, Node* left, Node* right)
    {
        if (!context->m_canSkipCopyToRegister) {
            return false;
        }

        std::vector<AtomicString> assignmentNames;
        std::vector<AtomicString> names;

        std::function<void(AtomicString name, bool isAssignment)> fn = [&assignmentNames, &names](AtomicString name, bool isAssignment) {
            if (isAssignment) {
                for (size_t i = 0; i < assignmentNames.size(); i++) {
                    if (assignmentNames[i] == name) {
                        return;
                    }
                }
                assignmentNames.push_back(name);
            } else {
                for (size_t i = 0; i < names.size(); i++) {
                    if (names[i] == name) {
                        return;
                    }
                }
                names.push_back(name);
            }
        };

        left->iterateChildrenIdentifier(fn);
        right->iterateChildrenIdentifier(fn);

        for (size_t i = 0; i < names.size(); i++) {
            for (size_t j = 0; j < assignmentNames.size(); j++) {
                if (names[i] == assignmentNames[j]) {
                    return false;
                }
            }
        }

        return true;
    }
};
}

#endif
