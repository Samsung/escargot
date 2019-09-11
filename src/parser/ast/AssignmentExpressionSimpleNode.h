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

#ifndef AssignmentExpressionSimpleNode_h
#define AssignmentExpressionSimpleNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "ArrayPatternNode.h"
#include "MemberExpressionNode.h"
#include "UnaryExpressionDeleteNode.h"
#include "CallExpressionNode.h"

namespace Escargot {

// An assignment operator expression.
class AssignmentExpressionSimpleNode : public ExpressionNode {
public:
    friend class ScriptParser;

    AssignmentExpressionSimpleNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    virtual ~AssignmentExpressionSimpleNode()
    {
    }

    void giveupChildren()
    {
        m_left = m_right = nullptr;
    }

    Node* left()
    {
        return m_left.get();
    }

    Node* right()
    {
        return m_right.get();
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentExpressionSimple; }
    static bool isLeftReferenceExpressionRelatedWithRightExpression(Node* left, Node* right)
    {
        std::vector<AtomicString> leftNames;
        left->iterateChildrenIdentifier([&leftNames](AtomicString name, bool isAssignment) {
            leftNames.push_back(name);
        });

        bool isSlowMode = false;
        right->iterateChildrenIdentifier([&leftNames, &isSlowMode](AtomicString name, bool isAssignment) {
            for (size_t i = 0; i < leftNames.size(); i++) {
                if (leftNames[i] == name) {
                    isSlowMode = true;
                }
            }
        });

        return isSlowMode;
    }

    static bool isLeftBindingAffectedByRightExpression(Node* left, Node* right)
    {
        if (left->isIdentifier()) {
            AtomicString leftName = left->asIdentifier()->name();
            bool result = false;
            right->iterateChildren([&](Node* node) {
                if (node->type() == ASTNodeType::UnaryExpressionDelete) {
                    UnaryExpressionDeleteNode* del = ((UnaryExpressionDeleteNode*)node);
                    if (del->argument()->isIdentifier()) {
                        if (leftName == del->argument()->asIdentifier()->name()) {
                            // x = delete x;
                            result = true;
                        }
                    }
                } else if (node->type() == ASTNodeType::CallExpression) {
                    CallExpressionNode* call = (CallExpressionNode*)node;
                    if (call->callee()->isIdentifier() && call->callee()->asIdentifier()->name().string()->equals("eval")) {
                        // x = (eval("var x;"), 1);
                        result = true;
                    }
                }
            });
            return result;
        }
        return false;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        bool isSlowMode = isLeftReferenceExpressionRelatedWithRightExpression(m_left.get(), m_right.get());

        if (codeBlock->m_codeBlock->hasEval()) {
            // x = (eval("var x;"), 1);
            isSlowMode = isSlowMode || isLeftBindingAffectedByRightExpression(m_left.get(), m_right.get());
        }

        bool isBase = context->m_registerStack->size() == 0;
        size_t rightRegister = dstRegister;

        if (isSlowMode) {
            bool oldCanSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            bool oldIsLeftBindingAffectedByRightExpression = context->m_isLeftBindingAffectedByRightExpression;
            context->m_isLeftBindingAffectedByRightExpression = isLeftBindingAffectedByRightExpression(m_left.get(), m_right.get());

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

    static void generateResultNotRequiredAssignmentByteCode(Node* self, Node* left, Node* right, ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;

        isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization && left->isIdentifier();

        bool isSlowMode = isLeftReferenceExpressionRelatedWithRightExpression(left, right);

        if (codeBlock->m_codeBlock->hasEval()) {
            // x = (eval("var x;"), 1);
            isSlowMode = isSlowMode || isLeftBindingAffectedByRightExpression(left, right);
        }

        if (isSlowMode) {
            size_t rightRegister = right->getRegister(codeBlock, context);
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            bool oldIsLeftBindingAffectedByRightExpression = context->m_isLeftBindingAffectedByRightExpression;
            context->m_isLeftBindingAffectedByRightExpression = isLeftBindingAffectedByRightExpression(left, right);

            left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            right->generateExpressionByteCode(codeBlock, context, rightRegister);

            if (isLexicallyDeclaredBindingInitialization) {
                context->addLexicallyDeclaredNames(left->asIdentifier()->name());
                context->m_isLexicallyDeclaredBindingInitialization = true;
            }
            left->generateStoreByteCode(codeBlock, context, rightRegister, false);

            context->m_isLeftBindingAffectedByRightExpression = oldIsLeftBindingAffectedByRightExpression;
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            context->giveUpRegister();
        } else {
            auto rt = right->type();
            if (left->isIdentifier() && (rt != ASTNodeType::RegisterReference && rt != ASTNodeType::ArrayExpression && rt != ASTNodeType::ObjectExpression) && !context->m_isGlobalScope && !context->m_isEvalCode) {
                auto r = left->asIdentifier()->isAllocatedOnStack(context);
                if (std::get<0>(r)) {
                    if (isLexicallyDeclaredBindingInitialization) {
                        context->addLexicallyDeclaredNames(left->asIdentifier()->name());
                    }
                    left->asIdentifier()->addLexicalVariableErrorsIfNeeds(codeBlock, context, std::get<2>(r), isLexicallyDeclaredBindingInitialization, true);
                    if (right->isLiteral()) {
                        auto r2 = right->getRegister(codeBlock, context);
                        if (r2 >= REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
                            context->giveUpRegister();
                            codeBlock->pushCode(Move(ByteCodeLOC(self->m_loc.index), r2, std::get<1>(r)), context, self);
                            return;
                        }
                    }
                    context->pushRegister(std::get<1>(r));
                    right->generateExpressionByteCode(codeBlock, context, std::get<1>(r));
                    context->giveUpRegister();
                    return;
                }
            }

            size_t rightRegister = right->getRegister(codeBlock, context);
            left->generateResolveAddressByteCode(codeBlock, context);
            right->generateExpressionByteCode(codeBlock, context, rightRegister);
            if (isLexicallyDeclaredBindingInitialization) {
                context->addLexicallyDeclaredNames(left->asIdentifier()->name());
                context->m_isLexicallyDeclaredBindingInitialization = true;
            }
            left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            context->giveUpRegister();
        }
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateResultNotRequiredAssignmentByteCode(this, m_left.get(), m_right.get(), codeBlock, context);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_left->iterateChildrenIdentifierAssigmentCase(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_left->iterateChildren(fn);
        m_right->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_left; // left: Pattern;
    RefPtr<Node> m_right; // right: Expression;
};
}

#endif
