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

#ifndef AssignmentExpressionSimpleNode_h
#define AssignmentExpressionSimpleNode_h

#include "AssignmentExpressionNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionSimpleNode : public AssignmentExpressionNode {
public:
    AssignmentExpressionSimpleNode(Node* left, Node* right)
        : AssignmentExpressionNode(left, right)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentExpressionSimple; }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

        bool isSlowMode = isLeftReferenceExpressionRelatedWithRightExpression();

        if (codeBlock->m_codeBlock->hasEval()) {
            // x = (eval("var x;"), 1);
            isSlowMode = isSlowMode || isLeftBindingAffectedByRightExpression();
        }

        bool isBase = context->m_registerStack->size() == 0;
        size_t rightRegister = dstRegister;

        if (isSlowMode) {
            bool oldCanSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            bool oldIsLeftBindingAffectedByRightExpression = context->m_isLeftBindingAffectedByRightExpression;
            context->m_isLeftBindingAffectedByRightExpression = isLeftBindingAffectedByRightExpression();

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = oldCanSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);

            context->m_isLeftBindingAffectedByRightExpression = oldIsLeftBindingAffectedByRightExpression;
        } else {
            m_left->generateResolveAddressByteCode(codeBlock, context);
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        }
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

        bool isSlowMode = isLeftReferenceExpressionRelatedWithRightExpression();

        if (codeBlock->m_codeBlock->hasEval()) {
            // x = (eval("var x;"), 1);
            isSlowMode = isSlowMode || isLeftBindingAffectedByRightExpression();
        }

        if (isSlowMode) {
            size_t rightRegister = m_right->getRegister(codeBlock, context);
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            bool oldIsLeftBindingAffectedByRightExpression = context->m_isLeftBindingAffectedByRightExpression;
            context->m_isLeftBindingAffectedByRightExpression = isLeftBindingAffectedByRightExpression();

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);

            context->m_isLeftBindingAffectedByRightExpression = oldIsLeftBindingAffectedByRightExpression;
            context->giveUpRegister();
        } else {
            auto rt = m_right->type();
            if (m_left->isIdentifier() && (rt != ASTNodeType::ArrayExpression && rt != ASTNodeType::ObjectExpression) && !context->shouldCareScriptExecutionResult()) {
                auto r = m_left->asIdentifier()->isAllocatedOnStack(context);
                if (std::get<0>(r)) {
                    m_left->asIdentifier()->addLexicalVariableErrorsIfNeeds(codeBlock, context, std::get<2>(r), false, true);
                    if (m_right->isLiteral()) {
                        auto r2 = m_right->getRegister(codeBlock, context);
                        if (r2 >= REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
                            context->giveUpRegister();
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r2, std::get<1>(r)), context, this->m_loc.index);
                            return;
                        }
                    }
                    context->pushRegister(std::get<1>(r));
                    m_right->generateExpressionByteCode(codeBlock, context, std::get<1>(r));
                    context->giveUpRegister();
                    return;
                }
            }

            size_t rightRegister = m_right->getRegister(codeBlock, context);
            m_left->generateResolveAddressByteCode(codeBlock, context);
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            context->giveUpRegister();
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        return context->getRegister();
    }
};
} // namespace Escargot

#endif
