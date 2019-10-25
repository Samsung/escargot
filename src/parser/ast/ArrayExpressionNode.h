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

#ifndef ArrayExpressionNode_h
#define ArrayExpressionNode_h

#include "ExpressionNode.h"
#include "ArrayPatternNode.h"
#include "runtime/Context.h"

namespace Escargot {

class ArrayExpressionNode : public ExpressionNode {
public:
    ArrayExpressionNode(NodeList& elements, AtomicString additionalPropertyName = AtomicString(), Node* additionalPropertyExpression = nullptr, bool hasSpreadElement = false, bool isTaggedTemplateExpression = false)
        : ExpressionNode()
        , m_elements(elements)
        , m_additionalPropertyName(additionalPropertyName)
        , m_additionalPropertyExpression(additionalPropertyExpression)
        , m_hasSpreadElement(hasSpreadElement)
        , m_isTaggedTemplateExpression(isTaggedTemplateExpression)
    {
    }

    NodeList& elements()
    {
        return m_elements;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ArrayExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        size_t arrayIndex = codeBlock->currentCodeSize();
        size_t arrLen = 0;
        codeBlock->pushCode(CreateArray(ByteCodeLOC(m_loc.index), dstRegister), context, this);
        size_t objIndex = dstRegister;

        size_t baseIndex = 0;
        SentinelNode* element = m_elements.begin();
        while (element != m_elements.end()) {
            size_t fillCount = 0;
            size_t regCount = 0;
            size_t regIndex = 0;
            ByteCodeRegisterIndex regs[ARRAY_DEFINE_OPERATION_MERGE_COUNT];
            while (regIndex < ARRAY_DEFINE_OPERATION_MERGE_COUNT && element != m_elements.end()) {
                ByteCodeRegisterIndex valueIndex = REGISTER_LIMIT;
                if (element->astNode()) {
                    valueIndex = element->astNode()->getRegister(codeBlock, context);
                    element->astNode()->generateExpressionByteCode(codeBlock, context, valueIndex);
                    regCount++;
                }
                regs[regIndex] = valueIndex;

                fillCount++;
                regIndex++;
                element = element->next();
            }

            if (m_hasSpreadElement) {
                codeBlock->pushCode(ArrayDefineOwnPropertyBySpreadElementOperation(ByteCodeLOC(m_loc.index), objIndex, fillCount), context, this);
                memcpy(codeBlock->peekCode<ArrayDefineOwnPropertyBySpreadElementOperation>(codeBlock->lastCodePosition<ArrayDefineOwnPropertyBySpreadElementOperation>())->m_loadRegisterIndexs,
                       regs, sizeof(regs));
            } else {
                codeBlock->pushCode(ArrayDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, baseIndex, fillCount), context, this);
                memcpy(codeBlock->peekCode<ArrayDefineOwnPropertyOperation>(codeBlock->lastCodePosition<ArrayDefineOwnPropertyOperation>())->m_loadRegisterIndexs,
                       regs, sizeof(regs));
            }

            for (size_t i = 0; i < regCount; i++) {
                // drop value register
                context->giveUpRegister();
            }

            baseIndex += fillCount;
            arrLen += fillCount;
        }

        if (!m_hasSpreadElement) {
            codeBlock->peekCode<CreateArray>(arrayIndex)->m_length = arrLen;
        }

        codeBlock->m_shouldClearStack = true;

        if (m_additionalPropertyExpression) {
            ByteCodeRegisterIndex additionalPropertyExpressionRegsiter = m_additionalPropertyExpression->getRegister(codeBlock, context);
            m_additionalPropertyExpression->generateExpressionByteCode(codeBlock, context, additionalPropertyExpressionRegsiter);
            codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), dstRegister, m_additionalPropertyName, additionalPropertyExpressionRegsiter, ObjectPropertyDescriptor::NotPresent), context, this);

            if (m_isTaggedTemplateExpression) {
                auto freezeFunctionRegister = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), freezeFunctionRegister, Value(codeBlock->m_codeBlock->context()->globalObject()->objectFreeze())), context, this);
                codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), freezeFunctionRegister, additionalPropertyExpressionRegsiter, freezeFunctionRegister, 1), context, this);
                context->giveUpRegister();
            }

            context->giveUpRegister();
        }

        if (m_isTaggedTemplateExpression) {
            auto freezeFunctionRegister = context->getRegister();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), freezeFunctionRegister, Value(codeBlock->m_codeBlock->context()->globalObject()->objectFreeze())), context, this);
            codeBlock->pushCode(CallFunction(ByteCodeLOC(m_loc.index), freezeFunctionRegister, dstRegister, freezeFunctionRegister, 1), context, this);
            context->giveUpRegister();
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            if (element->astNode()) {
                element->astNode()->iterateChildrenIdentifier(fn);
            }
        }
        if (m_additionalPropertyExpression) {
            m_additionalPropertyExpression->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* element = m_elements.begin(); element != m_elements.end(); element = element->next()) {
            if (element->astNode()) {
                element->astNode()->iterateChildren(fn);
            }
        }

        if (m_additionalPropertyExpression) {
            m_additionalPropertyExpression->iterateChildren(fn);
        }
    }

private:
    NodeList m_elements;
    AtomicString m_additionalPropertyName;
    Node* m_additionalPropertyExpression;
    bool m_hasSpreadElement;
    bool m_isTaggedTemplateExpression;
};
}

#endif
