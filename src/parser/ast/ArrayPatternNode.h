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
    friend class ScriptParser;
    ArrayPatternNode(ExpressionNodeVector&& elements)
        : m_elements(elements)
    {
    }

    ArrayPatternNode(ExpressionNodeVector&& elements, NodeLOC& loc)
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

        for (size_t i = 0; i < m_elements.size(); i++) {
            if (m_elements[i]) {
                if (LIKELY(m_elements[i]->type() != RestElement)) {
                    codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorIndex), context, this);
                    m_elements[i]->generateResolveAddressByteCode(codeBlock, context);
                    m_elements[i]->generateStoreByteCode(codeBlock, context, iteratorValueIndex, false);
                } else {
                    ASSERT(i == m_elements.size() - 1);
                    m_elements[i]->generateStoreByteCode(codeBlock, context, iteratorIndex, true);
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
        for (size_t i = 0; i < m_elements.size(); i++) {
            if (m_elements[i]) {
                m_elements[i]->iterateChildrenIdentifier(fn);
            }
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (size_t i = 0; i < m_elements.size(); i++) {
            if (m_elements[i])
                m_elements[i]->iterateChildren(fn);
        }
    }

private:
    ExpressionNodeVector m_elements;
};
}

#endif
