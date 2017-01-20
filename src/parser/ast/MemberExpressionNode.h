/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef MemberExpressionNode_h
#define MemberExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "runtime/Context.h"

namespace Escargot {

class MemberExpressionNode : public ExpressionNode {
public:
    MemberExpressionNode(Node* object, Node* property, bool computed)
        : ExpressionNode()
    {
        m_object = object;
        m_property = property;
        m_computed = computed;
    }

    virtual ~MemberExpressionNode()
    {
        delete m_object;
        delete m_property;
    }

    Node* object()
    {
        return m_object;
    }

    Node* property()
    {
        return m_property;
    }

    virtual ASTNodeType type() { return ASTNodeType::MemberExpression; }
    bool isPreComputedCase()
    {
        return m_computed;
    }

    AtomicString propertyName()
    {
        ASSERT(isPreComputedCase());
        return ((IdentifierNode*)m_property)->name();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        bool prevHead = context->m_isHeadOfMemberExpression;
        context->m_isHeadOfMemberExpression = false;
        m_object->generateExpressionByteCode(codeBlock, context);

        if (context->m_inCallingExpressionScope && prevHead) {
            size_t r0 = context->getLastRegisterIndex();
            size_t r1 = context->getRegister();
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r0, r1), context, this);
        }
        size_t objectIndex = context->getLastRegisterIndex();
        if (isPreComputedCase()) {
            ASSERT(m_property->isIdentifier());
            size_t pos = codeBlock->currentCodeSize();
            codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name()), context, this);
            codeBlock->m_literalData.pushBack((PointerValue*)codeBlock->peekCode<GetObjectPreComputedCase>(pos)->m_inlineCache.m_cache);
        } else {
            m_property->generateExpressionByteCode(codeBlock, context);
            codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex), context, this);
            context->giveUpRegister();
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, bool needToReferenceSelf = true)
    {
        if (isPreComputedCase()) {
            size_t valueIndex;
            size_t objectIndex;
            if (needToReferenceSelf) {
                valueIndex = context->getLastRegisterIndex();
                m_object->generateExpressionByteCode(codeBlock, context);
                objectIndex = context->getLastRegisterIndex();
            } else {
                valueIndex = context->getLastRegisterIndex();
                objectIndex = valueIndex - 1;
            }
            SetObjectInlineCache* inlineCache = new SetObjectInlineCache();
            codeBlock->m_literalData.pushBack((PointerValue*)inlineCache);
            codeBlock->pushCode(SetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name(), valueIndex, inlineCache), context, this);
            context->giveUpRegister();
        } else {
            size_t valueIndex;
            size_t objectIndex;
            size_t propertyIndex;
            if (needToReferenceSelf) {
                valueIndex = context->getLastRegisterIndex();
                m_object->generateExpressionByteCode(codeBlock, context);
                objectIndex = context->getLastRegisterIndex();
                m_property->generateExpressionByteCode(codeBlock, context);
                propertyIndex = context->getLastRegisterIndex();
            } else {
                valueIndex = context->getLastRegisterIndex();
                propertyIndex = valueIndex - 1;
                objectIndex = propertyIndex - 1;
            }
            codeBlock->pushCode(SetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this);
            context->giveUpRegister();
            context->giveUpRegister();
        }
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        m_object->generateExpressionByteCode(codeBlock, context);
        if (isPreComputedCase()) {
        } else {
            m_property->generateExpressionByteCode(codeBlock, context);
        }
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (isPreComputedCase()) {
            size_t objectIndex = context->getLastRegisterIndex();
            size_t r0 = objectIndex;
            size_t r1 = context->getRegister();
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), r0, r1), context, this);

            size_t pos = codeBlock->currentCodeSize();
            codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), r1, m_property->asIdentifier()->name()), context, this);
            codeBlock->m_literalData.pushBack((PointerValue*)codeBlock->peekCode<GetObjectPreComputedCase>(pos)->m_inlineCache.m_cache);
        } else {
            size_t objectIndex = context->getLastRegisterIndex() - 1;
            size_t propertyIndex = context->getLastRegisterIndex();
            size_t newObjectIndex = context->getRegister();
            size_t newPropertyIndex = context->getRegister();
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), objectIndex, newObjectIndex), context, this);
            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), propertyIndex, newPropertyIndex), context, this);
            codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), newObjectIndex), context, this);
            context->giveUpRegister();
        }
    }


protected:
    Node* m_object; // object: Expression;
    Node* m_property; // property: Identifier | Expression;

    bool m_computed;
};
}

#endif
