/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    {
        m_callee = callee;
        m_arguments = arguments;
    }

    virtual ~CallExpressionNode()
    {
        delete m_callee;
        for (size_t i = 0; i < m_arguments.size(); i++) {
            delete m_arguments[i];
        }
        m_arguments.clear();
    }
    Node* callee() { return m_callee; }
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

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (m_callee->isIdentifier() && m_callee->asIdentifier()->name().string()->equals("eval")) {
            size_t startIndex = generateArguments(codeBlock, context, false);
            codeBlock->pushCode(CallEvalFunction(ByteCodeLOC(m_loc.index), startIndex, m_arguments.size(), dstRegister), context, this);
            return;
        }

        bool prevInCallingExpressionScope = context->m_inCallingExpressionScope;
        bool useWithObjectAsReceiver = context->m_isWithScope && m_callee->isIdentifier();
        if (UNLIKELY(useWithObjectAsReceiver)) {
            // CallFunction should check whether receiver is global obj or with obj.
            ASSERT(m_callee->isIdentifier());
            AtomicString calleeName = m_callee->asIdentifier()->name();
            size_t startIndex = generateArguments(codeBlock, context);
            context->m_inCallingExpressionScope = prevInCallingExpressionScope;
            codeBlock->pushCode(CallFunctionInWithScope(ByteCodeLOC(m_loc.index), calleeName, startIndex, m_arguments.size(), dstRegister), context, this);
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

        if (isCalleeHasReceiver) {
            codeBlock->pushCode(CallFunctionWithReceiver(ByteCodeLOC(m_loc.index), receiverIndex, calleeIndex, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        } else {
            codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), calleeIndex, argumentsStartIndex, m_arguments.size(), dstRegister), context, this);
        }

        context->m_inCallingExpressionScope = prevInCallingExpressionScope;
    }

protected:
    Node* m_callee; // callee: Expression;
    ArgumentVector m_arguments; // arguments: [ Expression ];
};
}

#endif
