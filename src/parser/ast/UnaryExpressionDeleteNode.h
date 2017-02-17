/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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

#ifndef UnaryExpressionDeleteNode_h
#define UnaryExpressionDeleteNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionDeleteNode : public ExpressionNode {
public:
    friend class ScriptParser;
    UnaryExpressionDeleteNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = (ExpressionNode*)argument;
    }
    virtual ~UnaryExpressionDeleteNode()
    {
        delete m_argument;
    }

    virtual ASTNodeType type() { return ASTNodeType::UnaryExpressionDelete; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t base = context->m_baseRegisterCount;
        if (m_argument->isIdentifier()) {
            AtomicString name = m_argument->asIdentifier()->name();
            bool nameCase = false;
            if (!context->m_codeBlock->canUseIndexedVariableStorage()) {
                nameCase = true;
            } else {
                CodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(name);
                if (!info.m_isResultSaved) {
                    nameCase = true;
                }
            }

            if (nameCase) {
                codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), SIZE_MAX, SIZE_MAX, context->getRegister(), name), context, this);
            } else {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value(false)), context, this);
            }
        } else if (m_argument->isMemberExpression()) {
            ((MemberExpressionNode*)m_argument)->object()->generateExpressionByteCode(codeBlock, context);
            size_t o = context->getLastRegisterIndex();
            if (((MemberExpressionNode*)m_argument)->isPreComputedCase()) {
                // we can use LoadLiteral here
                // because, (MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string()
                // is protected by AtomicString (IdentifierNode always has AtomicString)
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value(((MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string())), context, this);
            } else
                ((MemberExpressionNode*)m_argument)->property()->generateExpressionByteCode(codeBlock, context);
            size_t p = context->getLastRegisterIndex();

            context->giveUpRegister();
            context->giveUpRegister();
            size_t dst = context->getRegister();

            codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), o, p, dst, AtomicString()), context, this);
        } else {
            m_argument->generateExpressionByteCode(codeBlock, context);
            context->giveUpRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value(true)), context, this);
        }

        ASSERT(base + 1 == context->m_baseRegisterCount);
    }

protected:
    ExpressionNode* m_argument;
};
}

#endif
