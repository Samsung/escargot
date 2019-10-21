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

#ifndef NewExpressionNode_h
#define NewExpressionNode_h

#include "ExpressionNode.h"
#include "CallExpressionNode.h"

namespace Escargot {

class NewExpressionNode : public ExpressionNode, public DestructibleNode {
public:
    using DestructibleNode::operator new;
    NewExpressionNode(Node* callee, NodeVector&& arguments)
        : ExpressionNode()
        , m_callee(callee)
        , m_arguments(arguments)
        , m_hasSpreadElement(false)
    {
    }

    virtual ~NewExpressionNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::NewExpression; }
    ByteCodeRegisterIndex generateArguments(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool clearInCallingExpressionScope = true)
    {
        ByteCodeRegisterIndex ret = context->getRegister();
        context->giveUpRegister();

        const unsigned smallAmountOfArguments = 16;
        if (m_arguments.size() && m_arguments.size() < smallAmountOfArguments) {
            ByteCodeRegisterIndex regs[smallAmountOfArguments];
            for (size_t i = 0; i < m_arguments.size(); i++) {
                regs[i] = m_arguments[i]->getRegister(codeBlock, context);
                if (m_arguments[i]->type() == ASTNodeType::SpreadElement) {
                    m_hasSpreadElement = true;
                }
            }

            bool isSorted = true;

            auto k = regs[0];
            for (size_t i = 1; i < m_arguments.size(); i++) {
                if (k + i != regs[i]) {
                    isSorted = false;
                    break;
                }
            }
            for (size_t i = 0; i < m_arguments.size(); i++) {
                context->giveUpRegister();
            }
            if (isSorted) {
                for (size_t i = 0; i < m_arguments.size(); i++) {
                    regs[i] = m_arguments[i]->getRegister(codeBlock, context);
                    m_arguments[i]->generateExpressionByteCode(codeBlock, context, regs[i]);
                }
                for (size_t i = 0; i < m_arguments.size(); i++) {
                    context->giveUpRegister();
                }
                return k;
            }
        }

        for (size_t i = 0; i < m_arguments.size(); i++) {
            size_t registerExpect = context->getRegister();
            m_arguments[i]->generateExpressionByteCode(codeBlock, context, registerExpect);
        }

        for (size_t i = 0; i < m_arguments.size(); i++) {
            context->giveUpRegister();
        }

        return ret;
    }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool isSlow = !CallExpressionNode::canUseDirectRegister(context, m_callee, m_arguments);
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }

        size_t callee = m_callee->getRegister(codeBlock, context);
        m_callee->generateExpressionByteCode(codeBlock, context, callee);

        size_t argumentsStartIndex = generateArguments(codeBlock, context);

        // give up callee index
        context->giveUpRegister();

        if (m_hasSpreadElement) {
            codeBlock->pushCode(NewOperationWithSpreadElement(ByteCodeLOC(m_loc.index), callee, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        } else {
            codeBlock->pushCode(NewOperation(ByteCodeLOC(m_loc.index), callee, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        }

        codeBlock->m_shouldClearStack = true;

        context->m_canSkipCopyToRegister = directBefore;
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_callee->iterateChildrenIdentifier(fn);
        for (size_t i = 0; i < m_arguments.size(); i++) {
            m_arguments[i]->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_callee->iterateChildren(fn);
        for (size_t i = 0; i < m_arguments.size(); i++) {
            m_arguments[i]->iterateChildren(fn);
        }
    }

private:
    Node* m_callee;
    NodeVector m_arguments;
    bool m_hasSpreadElement;
};
}

#endif
