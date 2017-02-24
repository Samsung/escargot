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
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
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
                codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), SIZE_MAX, SIZE_MAX, dstRegister, name), context, this);
            } else {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value(false)), context, this);
            }
        } else if (m_argument->isMemberExpression()) {
            ByteCodeRegisterIndex o = m_argument->getRegister(codeBlock, context);
            ((MemberExpressionNode*)m_argument)->object()->generateExpressionByteCode(codeBlock, context, o);
            ByteCodeRegisterIndex p;
            if (((MemberExpressionNode*)m_argument)->isPreComputedCase()) {
                // we can use LoadLiteral here
                // because, (MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string()
                // is protected by AtomicString (IdentifierNode always has AtomicString)
                p = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), p, Value(((MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string())), context, this);
            } else {
                p = ((MemberExpressionNode*)m_argument)->property()->getRegister(codeBlock, context);
                ((MemberExpressionNode*)m_argument)->property()->generateExpressionByteCode(codeBlock, context, p);
            }

            context->giveUpRegister();
            context->giveUpRegister();

            codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), o, p, dstRegister, AtomicString()), context, this);
        } else {
            ByteCodeRegisterIndex o = m_argument->getRegister(codeBlock, context);
            m_argument->generateExpressionByteCode(codeBlock, context, o);
            context->giveUpRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value(true)), context, this);
        }
    }

protected:
    ExpressionNode* m_argument;
};
}

#endif
