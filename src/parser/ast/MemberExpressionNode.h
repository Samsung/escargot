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

#ifndef MemberExpressionNode_h
#define MemberExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "ThisExpressionNode.h"

namespace Escargot {

class MemberExpressionNode : public ExpressionNode {
public:
    MemberExpressionNode(Node* object, Node* property, bool isPreComputedCase = false, bool isOptional = false, bool isReferencePrivateField = false)
        : ExpressionNode()
        , m_object(object)
        , m_property(property)
        , m_isPreComputedCase(isPreComputedCase)
        , m_isOptional(isOptional)
        , m_isReferencePrivateField(isReferencePrivateField)
        , m_startOfOptionalChaining(false)
    {
    }

    Node* object()
    {
        return m_object;
    }

    Node* property()
    {
        return m_property;
    }

    virtual ASTNodeType type() override { return ASTNodeType::MemberExpression; }

    inline AtomicString propertyName()
    {
        ASSERT(m_isPreComputedCase);
        return ((IdentifierNode*)m_property)->name();
    }

    inline bool hasInfinityPropertyName(ByteCodeBlock* codeBlock)
    {
        ASSERT(m_isPreComputedCase);
        return (propertyName() == codeBlock->m_codeBlock->context()->staticStrings().Infinity || propertyName() == codeBlock->m_codeBlock->context()->staticStrings().NegativeInfinity);
    }

    inline bool isPreComputedCase()
    {
        return m_isPreComputedCase;
    }

    inline bool isOptional()
    {
        return m_isOptional;
    }

    inline bool isReferencePrivateField()
    {
        return m_isReferencePrivateField;
    }

    virtual void setStartOfOptionalChaining() override
    {
        ASSERT(!m_startOfOptionalChaining);
        m_startOfOptionalChaining = true;
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        if (UNLIKELY(m_startOfOptionalChaining)) {
            context->pushOptionalChainingJumpPositionList();
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstIndex, Value()), context, this->m_loc.index);
        }

        bool prevHead = context->m_isHeadOfMemberExpression;
        context->m_isHeadOfMemberExpression = false;

        bool isSimple = true;

        if (!m_object->isIdentifier() || (!m_property->isLiteral() && !m_property->isIdentifier())) {
            isSimple = false;
        }

        size_t objectIndex;
        if (isSimple || m_isPreComputedCase) {
            objectIndex = m_object->getRegister(codeBlock, context);
        } else {
            objectIndex = context->getRegister();
        }

        // we should check super binding by loading this variable
        // for covering this case eg) super[super()]
        if (m_object->isSuperExpression() && !m_isPreComputedCase && codeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
            codeBlock->pushCode(LoadThisBinding(ByteCodeLOC(m_loc.index), objectIndex), context, this->m_loc.index);
        }

        m_object->generateExpressionByteCode(codeBlock, context, objectIndex);

        if (m_isOptional) {
            codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), false, objectIndex), context, this->m_loc.index);
            size_t optionalJumpPos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();
            context->addOptionalChainingJumpPosition(optionalJumpPos);
        }

        if (m_isPreComputedCase) {
            ASSERT(m_property->isIdentifier());
            if (UNLIKELY(m_object->isSuperExpression())) {
                size_t propertyIndex = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property->m_loc.index);
                codeBlock->pushCode(ComplexGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, propertyIndex), context, this->m_loc.index);
                context->giveUpRegister();
            } else if (UNLIKELY(hasInfinityPropertyName(codeBlock))) {
                // Handle Infinity property not to be inline-cached
                // because TypedArray has a corner case (TypedArrayObject get/set should be invoked)
                // e.g. typedarr.Infinity
                size_t propertyIndex = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property->m_loc.index);
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, dstIndex), context, this->m_loc.index);
                context->giveUpRegister();
            } else if (UNLIKELY(m_isReferencePrivateField)) {
                codeBlock->pushCode(ComplexGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, m_property->asIdentifier()->name(),
                                                              needsToReferOuterClassWhenEvaluatePrivateMember(context)),
                                    context, this->m_loc.index);
            } else {
                codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, m_property->asIdentifier()->name()), context, this->m_loc.index);
            }
        } else {
            size_t propertyIndex = m_property->getRegister(codeBlock, context);
            m_property->generateExpressionByteCode(codeBlock, context, propertyIndex);
            if (UNLIKELY(m_object->isSuperExpression())) {
                codeBlock->pushCode(ComplexGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, propertyIndex), context, this->m_loc.index);
            } else {
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, dstIndex), context, this->m_loc.index);
            }
            context->giveUpRegister();
        }

        if (!(context->m_inCallingExpressionScope && prevHead)) {
            context->giveUpRegister();
        }

        if (UNLIKELY(m_startOfOptionalChaining)) {
            context->linkOptionalChainingJumpPosition(codeBlock, codeBlock->currentCodeSize());
            context->popOptionalChainingJumpPositionList();
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex src, bool needToReferenceSelf) override
    {
        ASSERT(!m_isOptional);
        if (m_isPreComputedCase) {
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

            if (UNLIKELY(m_object->isSuperExpression())) {
                size_t propertyIndex = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property->m_loc.index);
                codeBlock->pushCode(ComplexSetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this->m_loc.index);
                context->giveUpRegister();
                context->giveUpRegister();
            } else if (UNLIKELY(hasInfinityPropertyName(codeBlock))) {
                // Handle Infinity property not to be inline-cached
                // because TypedArray has a corner case (TypedArrayObject get/set should be invoked)
                // e.g. typedarr.Infinity
                size_t propertyIndex = m_property->getRegister(codeBlock, context);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property->m_loc.index);
                codeBlock->pushCode(SetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this->m_loc.index);
                context->giveUpRegister();
                context->giveUpRegister();
            } else if (UNLIKELY(m_isReferencePrivateField)) {
                codeBlock->pushCode(ComplexSetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex,
                                                              m_property->asIdentifier()->name(), valueIndex,
                                                              needsToReferOuterClassWhenEvaluatePrivateMember(context)),
                                    context, this->m_loc.index);
                context->giveUpRegister();
            } else {
                codeBlock->pushCode(SetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, m_property->asIdentifier()->name(), valueIndex), context, this->m_loc.index);
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
            if (m_object->isSuperExpression()) {
                codeBlock->pushCode(ComplexSetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this->m_loc.index);
            } else {
                codeBlock->pushCode(SetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, valueIndex), context, this->m_loc.index);
            }
            context->giveUpRegister();
            context->giveUpRegister();
        }
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        // we should check super binding by loading this variable
        // for covering this case eg) super[super()]
        if (!m_isPreComputedCase && m_object->isSuperExpression() && codeBlock->m_codeBlock->needsToLoadThisBindingFromEnvironment()) {
            codeBlock->pushCode(LoadThisBinding(ByteCodeLOC(m_loc.index), context->getRegister()), context, this->m_loc.index);
            context->giveUpRegister();
        }

        ByteCodeRegisterIndex objectIndex = m_object->getRegister(codeBlock, context);
        m_object->generateExpressionByteCode(codeBlock, context, objectIndex);

        if (!m_isPreComputedCase) {
            ByteCodeRegisterIndex propertyIndex = m_property->getRegister(codeBlock, context);
            m_property->generateExpressionByteCode(codeBlock, context, propertyIndex);
        }
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (m_isPreComputedCase) {
            size_t objectIndex = context->getLastRegisterIndex();
            size_t resultIndex = context->getRegister();
            if (UNLIKELY(hasInfinityPropertyName(codeBlock))) {
                // Handle Infinity property not to be inline-cached
                // because TypedArray has a corner case (TypedArrayObject get/set should be invoked)
                // e.g. typedarr.Infinity
                size_t propertyIndex = m_property->getRegister(codeBlock, context);
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_property->asIdentifier()->m_loc.index), propertyIndex, m_property->asIdentifier()->name().string()), context, m_property->m_loc.index);
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, resultIndex), context, this->m_loc.index);
                context->giveUpRegister();
            } else if (UNLIKELY(m_isReferencePrivateField)) {
                codeBlock->pushCode(ComplexGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, resultIndex, m_property->asIdentifier()->name(),
                                                              needsToReferOuterClassWhenEvaluatePrivateMember(context)),
                                    context, this->m_loc.index);
            } else {
                codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objectIndex, resultIndex, m_property->asIdentifier()->name()), context, this->m_loc.index);
            }
        } else {
            size_t objectIndex = context->getLastRegisterIndex(1);
            size_t propertyIndex = context->getLastRegisterIndex();
            size_t dstIndex = context->getRegister();
            if (UNLIKELY(m_object->isSuperExpression())) {
                codeBlock->pushCode(ComplexGetObjectOperation(ByteCodeLOC(m_loc.index), objectIndex, dstIndex, propertyIndex), context, this->m_loc.index);
            } else {
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objectIndex, propertyIndex, dstIndex), context, this->m_loc.index);
            }
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_object->iterateChildrenIdentifier(fn);
        if (!m_isPreComputedCase) {
            m_property->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_object->iterateChildrenIdentifierAssigmentCase(fn);
        if (!m_isPreComputedCase) {
            m_property->iterateChildrenIdentifierAssigmentCase(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_object->iterateChildren(fn);
        m_property->iterateChildren(fn);
    }

    bool needsToReferOuterClassWhenEvaluatePrivateMember(ByteCodeGenerateContext* context)
    {
        ASSERT(m_isReferencePrivateField);

        auto pnames = context->m_codeBlock->classPrivateNames();
        if (pnames) {
            for (size_t i = 0; i < pnames->size(); i++) {
                AtomicString name = pnames->data()[i];

                InterpretedCodeBlock* c = context->m_codeBlock->parent();

                while (c) {
                    auto parentNames = c->classPrivateNames();
                    if (parentNames) {
                        for (size_t j = 0; j < parentNames->size(); j++) {
                            if (parentNames->data()[j] == name) {
                                return false;
                            }
                        }
                    }
                    c = c->parent();
                }
            }
        }

        return true;
    }

private:
    Node* m_object; // object: Expression;
    Node* m_property; // property: Identifier | Expression;

    bool m_isPreComputedCase;
    bool m_isOptional;
    bool m_isReferencePrivateField;
    bool m_startOfOptionalChaining;
};
} // namespace Escargot

#endif
