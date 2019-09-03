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

    virtual ASTNodeType type() { return ASTNodeType::ArrayPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
    {
        // store each element by iterator
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        bool hasRestElement = false;
        size_t elementsSize = m_elements.size();
        if (m_elements[elementsSize - 1] && m_elements[elementsSize - 1]->type() == RestElement) {
            hasRestElement = true;
        }
        size_t iteratorIndex = context->getRegister();
        size_t iteratorValueIndex = context->getRegister();

        codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index), srcRegister, iteratorIndex), context, this);

        for (size_t i = 0; i < m_elements.size() - 1; i++) {
            codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorIndex), context, this);
            if (m_elements[i]) {
                context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                m_elements[i]->generateResolveAddressByteCode(codeBlock, context);
                m_elements[i]->generateStoreByteCode(codeBlock, context, iteratorValueIndex, false);
            }
        }

        if (hasRestElement) {
            context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
            m_elements[elementsSize - 1]->generateStoreByteCode(codeBlock, context, iteratorIndex, true);
        } else if (m_elements[elementsSize - 1]) {
            codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIndex, iteratorIndex), context, this);
            context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
            m_elements[elementsSize - 1]->generateResolveAddressByteCode(codeBlock, context);
            m_elements[elementsSize - 1]->generateStoreByteCode(codeBlock, context, iteratorValueIndex, false);
        }

        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
        context->giveUpRegister(); // drop the iteratorValueIndex register
        context->giveUpRegister(); // drop the iteratorIndex register
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        for (size_t i = 0; i < m_elements.size(); i++) {
            if (m_elements[i]) {
                m_elements[i]->iterateChildrenIdentifier(fn);
            }
        }
    }

private:
    ExpressionNodeVector m_elements;
};
}

#endif
