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
            codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name()), context, this);
        } else {
            m_property->generateExpressionByteCode(codeBlock, context);
            codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex), context, this);
            context->giveUpRegister();
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (isPreComputedCase()) {
            size_t valueIndex = context->getLastRegisterIndex();
            m_object->generateExpressionByteCode(codeBlock, context);
            size_t objectIndex = context->getLastRegisterIndex();
            codeBlock->pushCode(SetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name(), valueIndex), context, this);
            context->giveUpRegister();
        } else {
            size_t valueIndex = context->getLastRegisterIndex();
            m_object->generateExpressionByteCode(codeBlock, context);
            size_t objectIndex = context->getLastRegisterIndex();
            m_property->generateExpressionByteCode(codeBlock, context);
            size_t propertyIndex = context->getLastRegisterIndex();
            codeBlock->pushCode(SetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this);
            context->giveUpRegister();
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
