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

#ifndef AssignmentPatternNode_h
#define AssignmentPatternNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"

#include "MemberExpressionNode.h"
#include "LiteralNode.h"

namespace Escargot {

class AssignmentPatternNode : public ExpressionNode {
public:
    AssignmentPatternNode(Node* left, Node* right)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
    }

    AssignmentPatternNode(Node* left, Node* right, NodeLOC& loc)
        : ExpressionNode()
        , m_left(left)
        , m_right(right)
    {
        m_loc = loc;
    }

    Node* left()
    {
        return m_left;
    }

    Node* right()
    {
        return m_right;
    }

    virtual ASTNodeType type() override { return ASTNodeType::AssignmentPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;

        LiteralNode* undefinedNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value());
        size_t undefinedIndex = undefinedNode->getRegister(codeBlock, context);
        undefinedNode->generateExpressionByteCode(codeBlock, context, undefinedIndex);

        size_t cmpIndex = context->getRegister();
        codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), srcRegister, undefinedIndex, cmpIndex), context, this->m_loc.index);

        context->giveUpRegister(); // for drop undefinedIndex

        codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), cmpIndex), context, this->m_loc.index);
        size_t pos1 = codeBlock->lastCodePosition<JumpIfTrue>();
        context->giveUpRegister(); // for drop cmpIndex

        // not undefined case, set srcRegister

        // for the case of function (x = x) {...}
        // set m_inParameterInitialization to false at first m_left->generateStoreByteCode, so addInitializedParameterNames is called by second m_left->generateStoreByteCode.
        // Only do this when m_left is a plain identifier (the self-reference case). For a
        // destructuring pattern left (e.g. function ({ [e.b]: e } = {}) {}) the computed
        // property keys are expressions that may reference not-yet-initialized parameters, so
        // the reference-before-initialization check must stay enabled while the pattern is stored.
        bool oldInParameterInitialization = context->m_inParameterInitialization;
        if (m_left->isIdentifier()) {
            context->m_inParameterInitialization = false;
        }

        // Snapshot the initialized parameter names: the "value provided" branch below and the
        // "default value" branch are mutually exclusive at runtime, so the default branch (and
        // the default expression m_right, which per spec is evaluated before the pattern is
        // bound) must not see the names bound by this branch as already initialized.
        std::vector<AtomicString> initializedParameterNamesBefore = context->m_initializedParameterNames;

        // Same reasoning for lexically declared names: the "value provided" branch below
        // initializes the bound name(s) (which records them as lexically declared, suppressing
        // the TDZ check for stack-allocated let/const bindings). The default expression m_right
        // is evaluated before the binding is initialized (e.g. for (let { a: n = typeof n } of x)),
        // so it must still see the name(s) as uninitialized. Snapshot here and restore before the
        // default branch below.
        std::vector<std::pair<size_t, AtomicString>> lexicallyDeclaredNamesBefore = *context->m_lexicallyDeclaredNames;

        m_left->generateResolveAddressByteCode(codeBlock, context);
        m_left->generateStoreByteCode(codeBlock, context, srcRegister, false);

        context->m_inParameterInitialization = oldInParameterInitialization;

        codeBlock->pushCode<Jump>(Jump(ByteCodeLOC(m_loc.index)), context, this->m_loc.index);
        size_t pos2 = codeBlock->lastCodePosition<Jump>();

        // undefined case, set default node
        codeBlock->peekCode<JumpIfTrue>(pos1)->m_jumpPosition = codeBlock->currentCodeSize();

        // restore initialized parameter names so the default branch re-checks parameter
        // references (e.g. the computed keys of the pattern) instead of reusing the state
        // accumulated by the "value provided" branch above.
        context->m_initializedParameterNames = std::move(initializedParameterNamesBefore);

        // restore lexically declared names so the default expression re-checks the temporal
        // dead zone (e.g. `typeof n` in the default of `n`) instead of reusing the state
        // accumulated by the "value provided" branch above.
        *context->m_lexicallyDeclaredNames = std::move(lexicallyDeclaredNamesBefore);

        size_t rightIndex = m_right->getRegister(codeBlock, context);
        m_left->generateResolveAddressByteCode(codeBlock, context);
        m_right->generateExpressionByteCode(codeBlock, context, rightIndex);

        // set m_isLexicallyDeclaredBindingInitialization for second m_left generateStoreByteCode
        context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
        m_left->generateStoreByteCode(codeBlock, context, rightIndex, false);
        context->giveUpRegister(); // for drop rightIndex

        codeBlock->peekCode<Jump>(pos2)->m_jumpPosition = codeBlock->currentCodeSize();

        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_left->iterateChildrenIdentifier(fn);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_left) {
            m_left->iterateChildren(fn);
        }

        if (m_right) {
            m_right->iterateChildren(fn);
        }
    }

private:
    Node* m_left; // left: Pattern;
    Node* m_right; // right: Expression;
};
} // namespace Escargot

#endif
