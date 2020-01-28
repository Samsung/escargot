/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ArrayPatternNode_h
#define ArrayPatternNode_h

#include "Node.h"
#include "LiteralNode.h"
#include "TryStatementNode.h"
#include "runtime/Context.h"

namespace Escargot {

class ArrayPatternNode : public Node {
public:
    ArrayPatternNode(const NodeList& elements)
        : m_elements(elements)
    {
    }

    ArrayPatternNode(const NodeList& elements, NodeLOC& loc)
        : m_elements(elements)
    {
        m_loc = loc;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ArrayPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        // store each element by iterator
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        size_t baseCountBefore = context->m_registerStack->size();

        // set m_isLexicallyDeclaredBindingInitialization to false for the case of empty array pattern
        context->m_isLexicallyDeclaredBindingInitialization = false;

        size_t iteratorRecordIndex = context->getRegister();
        size_t iteratorValueIndex = context->getRegister();

        codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index), srcRegister, iteratorRecordIndex), context, this);

        TryStatementNode::TryStatementByteCodeContext iteratorBindingContext;

        TryStatementNode::generateTryStatementStartByteCode(codeBlock, context, this, iteratorBindingContext);
        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            if (element->astNode()) {
                // enable lexical declaration only if each element exists
                context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                if (LIKELY(element->astNode()->type() != RestElement)) {
                    element->astNode()->generateResolveAddressByteCode(codeBlock, context);
                    codeBlock->pushCode(IteratorBind(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorRecordIndex), context, this);
                    element->astNode()->generateStoreByteCode(codeBlock, context, iteratorValueIndex, false);
                } else {
                    element->astNode()->generateStoreByteCode(codeBlock, context, iteratorRecordIndex, false);
                }
            } else {
                codeBlock->pushCode(IteratorBind(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorRecordIndex), context, this);
            }
        }
        context->giveUpRegister(); // for drop iteratorValueIndex

        TryStatementNode::generateTryStatementBodyEndByteCode(codeBlock, context, this, iteratorBindingContext);
        TryStatementNode::generateTryFinalizerStatementStartByteCode(codeBlock, context, this, iteratorBindingContext, true);
        // If iteratorRecord.[[Done]] is false, return ? IteratorClose(iteratorRecord, result).
        size_t doneIndex = context->getRegister();
        codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), iteratorRecordIndex, doneIndex, codeBlock->m_codeBlock->context()->staticStrings().done), context, this);
        codeBlock->pushCode(JumpIfTrue(ByteCodeLOC(m_loc.index), doneIndex), context, this);
        size_t jumpPos = codeBlock->lastCodePosition<JumpIfTrue>();
        codeBlock->pushCode(IteratorClose(ByteCodeLOC(m_loc.index), iteratorRecordIndex), context, this);
        codeBlock->peekCode<JumpIfTrue>(jumpPos)->m_jumpPosition = codeBlock->currentCodeSize();
        TryStatementNode::generateTryFinalizerStatementEndByteCode(codeBlock, context, this, iteratorBindingContext, true);

        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
        context->giveUpRegister(); // for drop doneIndex
        context->giveUpRegister(); // for drop iteratorRecordIndex

        codeBlock->m_shouldClearStack = true;

        ASSERT(context->m_registerStack->size() == baseCountBefore);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            if (element->astNode()) {
                element->astNode()->iterateChildrenIdentifier(fn);
            }
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            if (element->astNode())
                element->astNode()->iterateChildren(fn);
        }
    }

private:
    NodeList m_elements;
};
}

#endif
