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

#ifndef MemberExpressionNode_h
#define MemberExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "ThisExpressionNode.h"
#include "runtime/Context.h"

namespace Escargot {

class MemberExpressionNode : public ExpressionNode {
public:
    MemberExpressionNode(Node* object, Node* property, bool computed)
        : ExpressionNode()
        , m_object(object)
        , m_property(property)
        , m_computed(computed)
    {
    }

    virtual ~MemberExpressionNode()
    {
    }

    Node* object()
    {
        return m_object.get();
    }

    Node* property()
    {
        return m_property.get();
    }

    virtual ASTNodeType type() override { return ASTNodeType::MemberExpression; }
    bool isPreComputedCase()
    {
        return m_computed;
    }

    AtomicString propertyName()
    {
        ASSERT(isPreComputedCase());
        return ((IdentifierNode*)m_property.get())->name();
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        bool prevHead = context->m_isHeadOfMemberExpression;
        context->m_isHeadOfMemberExpression = false;

        bool isSimple = true;

        if (!m_object->isIdentifier() || (!m_property->isLiteral() && !m_property->isIdentifier())) {
            isSimple = false;
        }

        size_t objectIndex;
        if (isSimple || isPreComputedCase()) {
            objectIndex = m_object->getRegister(codeBlock, context);
        } else {
            objectIndex = context->getRegister();
        }

        m_object->generateExpressionByteCode(codeBlock, context, objectIndex);

        if (isPreComputedCase()) {
            ASSERT(m_property->isIdentifier());
            if (m_object->isSuperNode()) {
                size_t propertyIndex = m_property->getRegister(codeBlock, context);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property.get());
                codeBlock->pushCode(SuperGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, propertyIndex), context, this);
                context->giveUpRegister();
            } else {
                size_t pos = codeBlock->currentCodeSize();
                codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, m_property->asIdentifier()->name()), context, this);
                context->m_getObjectCodePositions.push_back(pos);
            }
        } else {
            size_t propertyIndex = m_property->getRegister(codeBlock, context);
            m_property->generateExpressionByteCode(codeBlock, context, propertyIndex);
            if (m_object->isSuperNode()) {
                codeBlock->pushCode(SuperGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, propertyIndex), context, this);
            } else {
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, dstIndex), context, this);
            }
            context->giveUpRegister();
        }

        if (!(context->m_inCallingExpressionScope && prevHead)) {
            context->giveUpRegister();
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex src, bool needToReferenceSelf) override
    {
        if (isPreComputedCase()) {
            size_t valueIndex;
            size_t objectIndex;
            if (needToReferenceSelf) {
                valueIndex = src;
                objectIndex = m_object->getRegister(codeBlock, context);
                m_object->generateExpressionByteCode(codeBlock, context, objectIndex);
            } else {
                valueIndex = src;
                objectIndex = context->getLastRegisterIndex();
            }

            if (m_object->isSuperNode()) {
                size_t propertyIndex = m_property->getRegister(codeBlock, context);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property.get());
                codeBlock->pushCode(SuperSetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this);
                context->giveUpRegister();
                context->giveUpRegister();
            } else {
                SetObjectInlineCache* inlineCache = new SetObjectInlineCache();
                codeBlock->m_literalData.pushBack(inlineCache);
                codeBlock->pushCode(SetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name(), valueIndex, inlineCache), context, this);
                context->giveUpRegister();
            }
        } else {
            size_t valueIndex;
            size_t objectIndex;
            size_t propertyIndex;
            if (needToReferenceSelf) {
                valueIndex = src;
                objectIndex = m_object->getRegister(codeBlock, context);
                m_object->generateExpressionByteCode(codeBlock, context, objectIndex);
                propertyIndex = m_property->getRegister(codeBlock, context);
                m_property->generateExpressionByteCode(codeBlock, context, propertyIndex);
                propertyIndex = context->getLastRegisterIndex();
            } else {
                valueIndex = src;
                propertyIndex = context->getLastRegisterIndex();
                objectIndex = context->getLastRegisterIndex(1);
            }
            if (m_object->isSuperNode()) {
                codeBlock->pushCode(SuperSetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this);
            } else {
                codeBlock->pushCode(SetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this);
            }
            context->giveUpRegister();
            context->giveUpRegister();
        }
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        ByteCodeRegisterIndex objectIndex = m_object->getRegister(codeBlock, context);
        m_object->generateExpressionByteCode(codeBlock, context, objectIndex);

        if (!isPreComputedCase()) {
            ByteCodeRegisterIndex propertyIndex = m_property->getRegister(codeBlock, context);
            m_property->generateExpressionByteCode(codeBlock, context, propertyIndex);
        }
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (isPreComputedCase()) {
            size_t objectIndex = context->getLastRegisterIndex();
            size_t resultIndex = context->getRegister();
            size_t pos = codeBlock->currentCodeSize();
            codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, resultIndex, m_property->asIdentifier()->name()), context, this);
            context->m_getObjectCodePositions.push_back(pos);
        } else {
            size_t objectIndex = context->getLastRegisterIndex(1);
            size_t propertyIndex = context->getLastRegisterIndex();
            size_t resultIndex = context->getRegister();
            codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, resultIndex), context, this);
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_object->iterateChildrenIdentifier(fn);
        if (!isPreComputedCase()) {
            m_property->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_object->iterateChildrenIdentifierAssigmentCase(fn);
        if (!isPreComputedCase()) {
            m_property->iterateChildrenIdentifierAssigmentCase(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_object->iterateChildren(fn);
        m_property->iterateChildren(fn);
    }

private:
    RefPtr<Node> m_object; // object: Expression;
    RefPtr<Node> m_property; // property: Identifier | Expression;

    bool m_computed : 1;
};
}

#endif
