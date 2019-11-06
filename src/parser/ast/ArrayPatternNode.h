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

        size_t iteratorIndex = context->getRegister();
        size_t iteratorValueIndex = context->getRegister();

        codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index), srcRegister, iteratorIndex), context, this);

        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
            if (element->astNode()) {
                if (LIKELY(element->astNode()->type() != RestElement)) {
                    codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorIndex), context, this);
                    element->astNode()->generateResolveAddressByteCode(codeBlock, context);
                    element->astNode()->generateStoreByteCode(codeBlock, context, iteratorValueIndex, false);
                } else {
                    element->astNode()->generateStoreByteCode(codeBlock, context, iteratorIndex, true);
                }
            } else {
                codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorIndex), context, this);
            }
        }

        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
        context->giveUpRegister(); // for drop iteratorValueIndex
        context->giveUpRegister(); // for drop iteratorIndex
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
