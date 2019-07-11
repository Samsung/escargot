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
#include "PatternNode.h"
#include "ArrayPatternNode.h"
#include "MemberExpressionNode.h"

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

    virtual ASTNodeType type() { return ASTNodeType::AssignmentExpressionSimple; }
    static bool hasSlowAssigmentOperation(Node* left, Node* right)
    {
        std::vector<AtomicString> leftNames;
        left->iterateChildrenIdentifier([&leftNames](AtomicString name, bool isAssgnment) {
            leftNames.push_back(name);
        });

        bool isSlowMode = false;
        right->iterateChildrenIdentifier([&leftNames, &isSlowMode](AtomicString name, bool isAssgnment) {
            for (size_t i = 0; i < leftNames.size(); i++) {
                if (leftNames[i] == name) {
                    isSlowMode = true;
                }
            }
        });

        return isSlowMode;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        context->m_classInfo.isAssigmentTarget = true;
        if (m_left->isPattern()) {
            Node* pattern = m_left.get()->asPattern(m_right);
            pattern->generateExpressionByteCode(codeBlock, context, dstRegister);
            return;
        }

        bool isSlowMode = hasSlowAssigmentOperation(m_left.get(), m_right.get());

        bool isBase = context->m_registerStack->size() == 0;
        size_t rightRegister = dstRegister;
        if (isSlowMode) {
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_classInfo.isAssigmentTarget = false;
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        } else {
            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_classInfo.isAssigmentTarget = false;
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
        }
    }

    class LexicallyDeclaredBindingNameAdder {
    public:
        LexicallyDeclaredBindingNameAdder(bool isLexicallyDeclaredBindingInitialization, AtomicString name, ByteCodeGenerateContext* context)
        {
            m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
            m_name = name;
            m_context = context;
        }

        ~LexicallyDeclaredBindingNameAdder()
        {
            if (m_isLexicallyDeclaredBindingInitialization) {
                m_context->m_lexicallyDeclaredNames->push_back(m_name);
            }
        }

        bool m_isLexicallyDeclaredBindingInitialization;
        AtomicString m_name;
        ByteCodeGenerateContext* m_context;
    };

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;

        if (m_left->isPattern()) {
            Node* pattern = m_left.get()->asPattern(m_right);
            (pattern)->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            return;
        }

        isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization && m_left->isIdentifier();
        LexicallyDeclaredBindingNameAdder nameAdder(isLexicallyDeclaredBindingInitialization, m_left->isIdentifier() ? m_left->asIdentifier()->name() : AtomicString(), context);

        bool isSlowMode = hasSlowAssigmentOperation(m_left.get(), m_right.get());
        context->m_classInfo.isAssigmentTarget = true;

        if (isSlowMode) {
            size_t rightRegister = m_right->getRegister(codeBlock, context);
            bool canSkipCopyToRegister = context->m_canSkipCopyToRegister;
            context->m_canSkipCopyToRegister = false;

            m_left->generateResolveAddressByteCode(codeBlock, context);
            context->m_canSkipCopyToRegister = canSkipCopyToRegister;
            context->m_classInfo.isAssigmentTarget = false;

            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            if (isLexicallyDeclaredBindingInitialization) {
                context->m_isLexicallyDeclaredBindingInitialization = true;
            }
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            context->giveUpRegister();
        } else {
            auto rt = m_right->type();
            if (m_left->isIdentifier() && (rt != ASTNodeType::RegisterReference && rt != ASTNodeType::ArrayExpression && rt != ASTNodeType::ObjectExpression) && !context->m_isGlobalScope && !context->m_isEvalCode) {
                auto r = m_left->asIdentifier()->isAllocatedOnStack(context);
                context->m_classInfo.isAssigmentTarget = false;
                if (std::get<0>(r)) {
                    m_left->asIdentifier()->addLexcialVariableErrorsIfNeeds(codeBlock, context, std::get<2>(r), isLexicallyDeclaredBindingInitialization, true);
                    if (m_right->isLiteral()) {
                        auto r2 = m_right->getRegister(codeBlock, context);
                        if (r2 >= REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
                            context->giveUpRegister();
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r2, std::get<1>(r)), context, this);
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
            context->m_classInfo.isAssigmentTarget = false;
            m_right->generateExpressionByteCode(codeBlock, context, rightRegister);
            if (isLexicallyDeclaredBindingInitialization) {
                context->m_isLexicallyDeclaredBindingInitialization = true;
            }
            m_left->generateStoreByteCode(codeBlock, context, rightRegister, false);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            context->giveUpRegister();
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_left->iterateChildrenIdentifierAssigmentCase(fn);
        m_right->iterateChildrenIdentifier(fn);
    }

private:
    RefPtr<Node> m_left; // left: Pattern;
    RefPtr<Node> m_right; // right: Expression;
};
}

#endif
