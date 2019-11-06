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

class NewExpressionNode : public ExpressionNode {
public:
    NewExpressionNode(Node* callee, const NodeList& arguments)
        : ExpressionNode()
        , m_callee(callee)
        , m_arguments(arguments)
        , m_hasSpreadElement(false)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::NewExpression; }
    ByteCodeRegisterIndex generateArguments(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool clearInCallingExpressionScope = true)
    {
        ByteCodeRegisterIndex ret = context->getRegister();
        context->giveUpRegister();

        const unsigned smallAmountOfArguments = 16;
        if (m_arguments.size() && m_arguments.size() < smallAmountOfArguments) {
            bool isSorted = true;
            ByteCodeRegisterIndex regs[smallAmountOfArguments];
            size_t regIndex = 0;
            for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
                regs[regIndex] = argument->astNode()->getRegister(codeBlock, context);
                if (regs[regIndex] != regs[0] + regIndex) {
                    isSorted = false;
                }
                if (argument->astNode()->type() == ASTNodeType::SpreadElement) {
                    m_hasSpreadElement = true;
                }
                regIndex++;
            }

            if (isSorted) {
                regIndex = 0;
                for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
                    argument->astNode()->generateExpressionByteCode(codeBlock, context, regs[regIndex]);
                    regIndex++;
                }
            }

            for (size_t i = 0; i < m_arguments.size(); i++) {
                context->giveUpRegister();
            }

            if (isSorted) {
                return regs[0];
            }
        }

        for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
            size_t registerExpect = context->getRegister();
            argument->astNode()->generateExpressionByteCode(codeBlock, context, registerExpect);
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
        for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
            argument->astNode()->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_callee->iterateChildren(fn);
        for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
            argument->astNode()->iterateChildren(fn);
        }
    }

private:
    Node* m_callee;
    NodeList m_arguments;
    bool m_hasSpreadElement;
};
}

#endif
