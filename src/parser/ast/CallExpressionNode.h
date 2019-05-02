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
#include "PatternNode.h"

namespace Escargot {

class CallExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    CallExpressionNode(Node* callee, ArgumentVector&& arguments)
        : ExpressionNode()
        , m_callee(callee)
        , m_arguments(std::move(arguments))
        , m_hasSpreadElement(false)
    {
    }

    virtual ~CallExpressionNode()
    {
    }

    Node* callee() { return m_callee.get(); }
    virtual ASTNodeType type() { return ASTNodeType::CallExpression; }
    ByteCodeRegisterIndex generateArguments(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool clearInCallingExpressionScope = true)
    {
        context->m_inCallingExpressionScope = !clearInCallingExpressionScope;

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

    static bool canUseDirectRegister(ByteCodeGenerateContext* context, Node* callee, const ArgumentVector& args)
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
        for (size_t i = 0; i < args.size(); i++) {
            args[i]->iterateChildrenIdentifier(fn);
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

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (m_callee->isIdentifier() && m_callee->asIdentifier()->name().string()->equals("eval")) {
            size_t evalIndex = context->getRegister();
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), evalIndex, codeBlock->m_codeBlock->context()->staticStrings().eval), context, this);
            size_t startIndex = generateArguments(codeBlock, context, false);
            context->giveUpRegister();
            codeBlock->pushCode(CallEvalFunction(ByteCodeLOC(m_loc.index), evalIndex, startIndex, m_arguments.size(), dstRegister, context->m_isWithScope, m_hasSpreadElement), context, this);
            return;
        }

        bool isSlow = !canUseDirectRegister(context, m_callee.get(), m_arguments);
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
            size_t startIndex = generateArguments(codeBlock, context);
            context->m_inCallingExpressionScope = prevInCallingExpressionScope;
            codeBlock->pushCode(CallFunctionInWithScope(ByteCodeLOC(m_loc.index), calleeName, startIndex, m_arguments.size(), dstRegister, m_hasSpreadElement), context, this);
            return;
        }

        bool isCalleeHasReceiver = false;
        if (m_callee->isMemberExpression()) {
            isCalleeHasReceiver = true;
            context->m_inCallingExpressionScope = true;
            context->m_isHeadOfMemberExpression = true;
        }

        size_t receiverIndex = SIZE_MAX;
        size_t calleeIndex = SIZE_MAX;

        calleeIndex = m_callee->getRegister(codeBlock, context);
        m_callee->generateExpressionByteCode(codeBlock, context, calleeIndex);


        if (isCalleeHasReceiver) {
            receiverIndex = context->getLastRegisterIndex();
        }

        size_t argumentsStartIndex = generateArguments(codeBlock, context);

        // drop callee, receiver registers
        if (isCalleeHasReceiver) {
            context->giveUpRegister();
            context->giveUpRegister();
        } else {
            context->giveUpRegister();
        }

        if (m_hasSpreadElement) {
            codeBlock->pushCode(CallFunctionWithSpreadElement(ByteCodeLOC(m_loc.index), receiverIndex, calleeIndex, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        } else if (isCalleeHasReceiver) {
            codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), receiverIndex, calleeIndex, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        } else {
            codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), calleeIndex, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        }

        context->m_inCallingExpressionScope = prevInCallingExpressionScope;

        context->m_canSkipCopyToRegister = directBefore;
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_callee->iterateChildrenIdentifier(fn);
        for (size_t i = 0; i < m_arguments.size(); i++) {
            m_arguments[i]->iterateChildrenIdentifier(fn);
        }
    }

private:
    RefPtr<Node> m_callee; // callee: Expression;
    ArgumentVector m_arguments; // arguments: [ Expression ];
    bool m_hasSpreadElement : 1;
};
}

#endif
