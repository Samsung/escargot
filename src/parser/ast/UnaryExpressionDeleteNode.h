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

#ifndef UnaryExpressionDeleteNode_h
#define UnaryExpressionDeleteNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionDeleteNode : public ExpressionNode {
public:
    explicit UnaryExpressionDeleteNode(Node* argument)
        : ExpressionNode()
        , m_argument(argument)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::UnaryExpressionDelete; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool hasSuperExpression = false;
        if (m_argument->isIdentifier()) {
            AtomicString name = m_argument->asIdentifier()->name();
            bool nameCase = false;
            if (!context->m_codeBlock->canUseIndexedVariableStorage()) {
                nameCase = true;
            } else {
                InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->indexedIdentifierInfo(name, context);
                if (!info.m_isResultSaved) {
                    nameCase = true;
                }
            }

            if (nameCase) {
                codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), SIZE_MAX, SIZE_MAX, dstRegister, name, hasSuperExpression), context, this);
            } else {
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value(false)), context, this);
            }
        } else if (m_argument->isMemberExpression()) {
            hasSuperExpression = ((MemberExpressionNode*)m_argument)->object()->isSuperExpression();
            if (hasSuperExpression) {
                ByteCodeRegisterIndex p;
                if (((MemberExpressionNode*)m_argument)->isPreComputedCase()) {
                    p = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), p, Value(((MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string())), context, this);
                } else {
                    p = ((MemberExpressionNode*)m_argument)->property()->getRegister(codeBlock, context);
                    ((MemberExpressionNode*)m_argument)->property()->generateExpressionByteCode(codeBlock, context, p);
                }
                context->giveUpRegister();
                codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), 0, p, dstRegister, AtomicString(), hasSuperExpression), context, this);
            } else {
                ByteCodeRegisterIndex o = m_argument->getRegister(codeBlock, context);
                ((MemberExpressionNode*)m_argument)->object()->generateExpressionByteCode(codeBlock, context, o);
                ByteCodeRegisterIndex p;
                if (((MemberExpressionNode*)m_argument)->isPreComputedCase()) {
                    // we can use LoadLiteral here
                    // because, (MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string()
                    // is private by AtomicString (IdentifierNode always has AtomicString)
                    p = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), p, Value(((MemberExpressionNode*)m_argument)->property()->asIdentifier()->name().string())), context, this);
                } else {
                    p = ((MemberExpressionNode*)m_argument)->property()->getRegister(codeBlock, context);
                    ((MemberExpressionNode*)m_argument)->property()->generateExpressionByteCode(codeBlock, context, p);
                }

                context->giveUpRegister();
                context->giveUpRegister();
                codeBlock->pushCode(UnaryDelete(ByteCodeLOC(m_loc.index), o, p, dstRegister, AtomicString(), hasSuperExpression), context, this);
            }
        } else {
            ByteCodeRegisterIndex o = m_argument->getRegister(codeBlock, context);
            m_argument->generateExpressionByteCode(codeBlock, context, o);
            context->giveUpRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value(true)), context, this);
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_argument->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_argument->iterateChildren(fn);
    }

    ExpressionNode* argument()
    {
        return (ExpressionNode*)m_argument;
    }

private:
    Node* m_argument;
};
} // namespace Escargot

#endif
