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

#ifndef CallExpressionNode_h
#define CallExpressionNode_h

#include "ExpressionNode.h"
#include "MemberExpressionNode.h"

#include "SuperExpressionNode.h"

namespace Escargot {

class CallExpressionNode : public ExpressionNode {
public:
    CallExpressionNode(Node* callee, NodeList& arguments)
        : ExpressionNode()
        , m_callee(callee)
        , m_arguments(arguments)
    {
    }

    Node* callee() { return m_callee; }
    virtual ASTNodeType type() override { return ASTNodeType::CallExpression; }
    std::pair<ByteCodeRegisterIndex, bool> generateArguments(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool clearInCallingExpressionScope = true)
    {
        context->m_inCallingExpressionScope = !clearInCallingExpressionScope;
        static const unsigned smallAmountOfArguments = 16;

        if (m_arguments.size() > 0) {
            bool hasSpreadElement = false;

            if (m_arguments.size() < smallAmountOfArguments) {
                bool isSorted = true;
                ByteCodeRegisterIndex regs[smallAmountOfArguments];
                size_t regIndex = 0;
                for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
                    regs[regIndex] = argument->astNode()->getRegister(codeBlock, context);
                    if (regs[regIndex] != regs[0] + regIndex) {
                        isSorted = false;
                    }
                    if (argument->astNode()->type() == ASTNodeType::SpreadElement) {
                        hasSpreadElement = true;
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
                    return std::make_pair(regs[0], hasSpreadElement);
                }
            }

            ByteCodeRegisterIndex argStartIndex = context->getRegister();
            context->giveUpRegister();
            for (SentinelNode* argument = m_arguments.begin(); argument != m_arguments.end(); argument = argument->next()) {
                if (argument->astNode()->type() == ASTNodeType::SpreadElement) {
                    hasSpreadElement = true;
                }
                ByteCodeRegisterIndex argIndex = context->getRegister();
                argument->astNode()->generateExpressionByteCode(codeBlock, context, argIndex);
            }
            for (size_t i = 0; i < m_arguments.size(); i++) {
                context->giveUpRegister();
            }
            return std::make_pair(argStartIndex, hasSpreadElement);
        }

        return std::make_pair(REGISTER_LIMIT, false);
    }

    static bool canUseDirectRegister(ByteCodeGenerateContext* context, Node* callee, NodeList& args)
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

        callee->iterateChildrenIdentifier(fn);
        for (SentinelNode* argument = args.begin(); argument != args.end(); argument = argument->next()) {
            argument->astNode()->iterateChildrenIdentifier(fn);
        }

        for (size_t i = 0; i < names.size(); i++) {
            for (size_t j = 0; j < assignmentNames.size(); j++) {
                if (names[i] == assignmentNames[j]) {
                    return false;
                }
            }
        }
        return true;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        if (m_callee->isIdentifier() && m_callee->asIdentifier()->name().string()->equals("eval")) {
            ByteCodeRegisterIndex evalIndex = context->getRegister();
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), evalIndex, codeBlock->m_codeBlock->context()->staticStrings().eval), context, this);
            auto args = generateArguments(codeBlock, context, false);
            ByteCodeRegisterIndex startIndex = args.first;
            context->giveUpRegister();
            codeBlock->pushCode(CallEvalFunction(ByteCodeLOC(m_loc.index), evalIndex, startIndex, dstRegister, m_arguments.size(), context->m_isWithScope, args.second), context, this);
            return;
        }

        bool isSlow = !canUseDirectRegister(context, m_callee, m_arguments);
        bool directBefore = context->m_canSkipCopyToRegister;
        if (isSlow) {
            context->m_canSkipCopyToRegister = false;
        }

        bool prevInCallingExpressionScope = context->m_inCallingExpressionScope;
        bool useWithObjectAsReceiver = context->m_isWithScope && m_callee->isIdentifier();
        if (UNLIKELY(useWithObjectAsReceiver)) {
            // CallFunction should check whether receiver is global obj or with obj.
            ASSERT(m_callee->isIdentifier());
            AtomicString calleeName = m_callee->asIdentifier()->name();
            auto args = generateArguments(codeBlock, context);
            ByteCodeRegisterIndex startIndex = args.first;
            context->m_inCallingExpressionScope = prevInCallingExpressionScope;
            codeBlock->pushCode(CallFunctionInWithScope(ByteCodeLOC(m_loc.index), calleeName, startIndex, dstRegister, m_arguments.size(), args.second), context, this);
            return;
        }

        bool isCalleeHasReceiver = false;
        bool isSuperCall = m_callee->isSuperExpression() && ((SuperExpressionNode*)m_callee)->isCall();

        if (m_callee->isMemberExpression()) {
            isCalleeHasReceiver = true;
            context->m_inCallingExpressionScope = true;
            context->m_isHeadOfMemberExpression = true;
        }

        ByteCodeRegisterIndex receiverIndex = REGISTER_LIMIT;
        ByteCodeRegisterIndex calleeIndex = REGISTER_LIMIT;

        calleeIndex = m_callee->getRegister(codeBlock, context);
        m_callee->generateExpressionByteCode(codeBlock, context, calleeIndex);


        if (isCalleeHasReceiver) {
            receiverIndex = context->getLastRegisterIndex();
            Node* object = ((MemberExpressionNode*)(m_callee))->object();
            if (object->isSuperExpression()) {
                ThisExpressionNode* nd = new (alloca(sizeof(ThisExpressionNode))) ThisExpressionNode();
                nd->generateExpressionByteCode(codeBlock, context, receiverIndex);
            }
        }

        auto args = generateArguments(codeBlock, context);
        ByteCodeRegisterIndex argumentsStartIndex = args.first;
        bool hasSpreadElement = args.second;

        // drop callee, receiver registers
        if (isCalleeHasReceiver) {
            context->giveUpRegister();
            context->giveUpRegister();
        } else {
            context->giveUpRegister();
        }

        if (isSuperCall) {
            codeBlock->pushCode(CallSuper(ByteCodeLOC(m_loc.index), calleeIndex, argumentsStartIndex, dstRegister, m_arguments.size(), hasSpreadElement), context, this);
        } else if (hasSpreadElement) {
            codeBlock->pushCode(CallFunctionWithSpreadElement(ByteCodeLOC(m_loc.index), receiverIndex, calleeIndex, argumentsStartIndex, dstRegister, m_arguments.size()), context, this);
        } else if (isCalleeHasReceiver) {
            codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), receiverIndex, calleeIndex, argumentsStartIndex, dstRegister, m_arguments.size()), context, this);
        } else {
            codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), calleeIndex, argumentsStartIndex, dstRegister, m_arguments.size()), context, this);
        }

        context->m_inCallingExpressionScope = prevInCallingExpressionScope;

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
    Node* m_callee; // callee: Expression;
    NodeList m_arguments; // arguments: [ Expression ];
};
}

#endif
