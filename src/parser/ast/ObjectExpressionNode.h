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

#ifndef ObjectExpressionNode_h
#define ObjectExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "ObjectPatternNode.h"
#include "PropertyNode.h"
#include "SpreadElementNode.h"

namespace Escargot {

class ObjectExpressionNode : public ExpressionNode {
public:
    explicit ObjectExpressionNode(const NodeList& properties)
        : ExpressionNode()
        , m_properties(properties)
    {
    }

    NodeList& properties()
    {
        return m_properties;
    }

    size_t generateInitValueByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, PropertyNode* p, ByteCodeRegisterIndex dstRegister)
    {
        size_t valueIndex = p->value()->getRegister(codeBlock, context);
        const ClassContextInformation classInfoBefore = context->m_classInfo;
        context->m_classInfo.m_prototypeIndex = dstRegister;
        context->m_classInfo.m_constructorIndex = SIZE_MAX;
        context->m_classInfo.m_superIndex = SIZE_MAX;
        p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);
        context->m_classInfo = classInfoBefore;
        return valueIndex;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ObjectExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        if (m_properties.size() >= ESCARGOT_OBJECT_STRUCTURE_TRANSITION_MODE_MAX_SIZE) {
            size_t objectCreationDataIndex = context->getRegister();
            size_t initCodePosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(CreateObjectPrepare(ByteCodeLOC(m_loc.index), objectCreationDataIndex, dstRegister), context, this->m_loc.index);

            size_t propertyIndex = 0;
            for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); propertyIndex++, property = property->next()) {
                if (property->astNode()->isProperty()) {
                    PropertyNode* p = property->astNode()->asProperty();
                    bool hasKeyName = false;
                    size_t propertyIndex = SIZE_MAX;
                    if (p->key()->isIdentifier() && !p->computed()) {
                        hasKeyName = true;
                        propertyIndex = context->getRegister();
                        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), propertyIndex, Value(p->key()->asIdentifier()->name().string())), context, this->m_loc.index);
                    } else {
                        propertyIndex = p->key()->getRegister(codeBlock, context);
                        p->key()->generateExpressionByteCode(codeBlock, context, propertyIndex);
                    }

                    size_t valueIndex = generateInitValueByteCode(codeBlock, context, p, dstRegister);
                    if (p->kind() == PropertyNode::Kind::Init) {
                        bool hasFunctionOnRightSide = p->value()->type() == ASTNodeType::FunctionExpression || p->value()->type() == ASTNodeType::ArrowFunctionExpression;
                        bool hasClassOnRightSide = p->value()->type() == ASTNodeType::ClassExpression && !p->value()->asClassExpression()->classNode().classBody()->hasStaticMemberName(codeBlock->m_codeBlock->context()->staticStrings().name);
                        codeBlock->pushCode(CreateObjectPrepare(ByteCodeLOC(m_loc.index), hasKeyName, objectCreationDataIndex, propertyIndex, valueIndex, !hasKeyName && (hasFunctionOnRightSide | hasClassOnRightSide)), context, this->m_loc.index);
                    } else {
                        ASSERT(p->kind() == PropertyNode::Kind::Get || p->kind() == PropertyNode::Kind::Set);
                        codeBlock->pushCode(CreateObjectPrepare(ByteCodeLOC(m_loc.index), hasKeyName, p->kind() == PropertyNode::Kind::Get, objectCreationDataIndex, propertyIndex, valueIndex), context, this->m_loc.index);
                    }

                    context->giveUpRegister(); // value
                    context->giveUpRegister(); // key
                } else {
                    // handle spread element in Object initialization
                    ASSERT(property->astNode()->type() == ASTNodeType::SpreadElement);
                    Node* element = property->astNode()->asSpreadElement()->argument();

                    ByteCodeRegisterIndex elementIndex = element->getRegister(codeBlock, context);
                    ByteCodeRegisterIndex dataIndex = context->getRegister();
                    ByteCodeRegisterIndex keyIndex = context->getRegister();
                    ByteCodeRegisterIndex valueIndex = context->getRegister();

                    element->generateExpressionByteCode(codeBlock, context, elementIndex);

                    codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), false, elementIndex), context, this->m_loc.index);
                    size_t pos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();

                    codeBlock->pushCode(CreateEnumerateObject(ByteCodeLOC(m_loc.index), elementIndex, dataIndex, true), context, this->m_loc.index);

                    size_t checkPos = codeBlock->currentCodeSize();
                    codeBlock->pushCode(CheckLastEnumerateKey(ByteCodeLOC(m_loc.index)), context, this->m_loc.index);
                    codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_registerIndex = dataIndex;
                    codeBlock->pushCode(GetEnumerateKey(ByteCodeLOC(m_loc.index), keyIndex, dataIndex), context, this->m_loc.index);

                    codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), elementIndex, keyIndex, valueIndex), context, this->m_loc.index);
                    codeBlock->pushCode(CreateObjectPrepare(ByteCodeLOC(m_loc.index), false, objectCreationDataIndex, keyIndex, valueIndex, false), context, this->m_loc.index);
                    codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), checkPos), context, this->m_loc.index);

                    size_t exitPos = codeBlock->currentCodeSize();
                    codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_exitPosition = exitPos;

                    // for drop elementIndex, dataIndex, keyIndex, valueIndex
                    context->giveUpRegister();
                    context->giveUpRegister();
                    context->giveUpRegister();
                    context->giveUpRegister();

                    codeBlock->peekCode<JumpIfUndefinedOrNull>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
                }
            }

            codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), dstRegister, objectCreationDataIndex), context, this->m_loc.index);
            codeBlock->peekCode<CreateObjectPrepare>(initCodePosition)->m_propertyReserveSize = propertyIndex;
            context->giveUpRegister();
            codeBlock->m_shouldClearStack = true;
            return;
        }

        codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), dstRegister), context, this->m_loc.index);
        size_t objIndex = dstRegister;
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            if (property->astNode()->isProperty()) {
                PropertyNode* p = property->astNode()->asProperty();
                bool hasKeyName = false;
                size_t propertyIndex = SIZE_MAX;
                if (p->key()->isIdentifier() && !p->computed()) {
                    hasKeyName = true;
                } else {
                    propertyIndex = p->key()->getRegister(codeBlock, context);
                    p->key()->generateExpressionByteCode(codeBlock, context, propertyIndex);
                }

                size_t valueIndex = generateInitValueByteCode(codeBlock, context, p, dstRegister);

                if (p->kind() == PropertyNode::Kind::Init) {
                    if (hasKeyName) {
                        codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), objIndex, p->key()->asIdentifier()->name(), valueIndex, ObjectPropertyDescriptor::AllPresent), context, this->m_loc.index);
                    } else {
                        bool hasFunctionOnRightSide = p->value()->type() == ASTNodeType::FunctionExpression || p->value()->type() == ASTNodeType::ArrowFunctionExpression;
                        bool hasClassOnRightSide = p->value()->type() == ASTNodeType::ClassExpression && !p->value()->asClassExpression()->classNode().classBody()->hasStaticMemberName(codeBlock->m_codeBlock->context()->staticStrings().name);
                        codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, ObjectPropertyDescriptor::AllPresent, hasFunctionOnRightSide | hasClassOnRightSide), context, this->m_loc.index);
                    }
                } else if (p->kind() == PropertyNode::Kind::Get) {
                    codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent), true, hasKeyName), context, this->m_loc.index);
                } else {
                    ASSERT(p->kind() == PropertyNode::Kind::Set);
                    codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent), false, hasKeyName), context, this->m_loc.index);
                }

                if (!hasKeyName) {
                    context->giveUpRegister(); // for drop property index
                }

                context->giveUpRegister(); // for drop value index
            } else {
                // handle spread element in Object initialization
                ASSERT(property->astNode()->type() == ASTNodeType::SpreadElement);
                Node* element = property->astNode()->asSpreadElement()->argument();

                ByteCodeRegisterIndex elementIndex = element->getRegister(codeBlock, context);
                ByteCodeRegisterIndex dataIndex = context->getRegister();
                ByteCodeRegisterIndex keyIndex = context->getRegister();
                ByteCodeRegisterIndex valueIndex = context->getRegister();

                element->generateExpressionByteCode(codeBlock, context, elementIndex);

                codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), false, elementIndex), context, this->m_loc.index);
                size_t pos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();

                codeBlock->pushCode(CreateEnumerateObject(ByteCodeLOC(m_loc.index), elementIndex, dataIndex, true), context, this->m_loc.index);

                size_t checkPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(CheckLastEnumerateKey(ByteCodeLOC(m_loc.index)), context, this->m_loc.index);
                codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_registerIndex = dataIndex;
                codeBlock->pushCode(GetEnumerateKey(ByteCodeLOC(m_loc.index), keyIndex, dataIndex), context, this->m_loc.index);

                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), elementIndex, keyIndex, valueIndex), context, this->m_loc.index);
                codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, keyIndex, valueIndex, ObjectPropertyDescriptor::AllPresent, false), context, this->m_loc.index);
                codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), checkPos), context, this->m_loc.index);

                size_t exitPos = codeBlock->currentCodeSize();
                codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_exitPosition = exitPos;

                // for drop elementIndex, dataIndex, keyIndex, valueIndex
                context->giveUpRegister();
                context->giveUpRegister();
                context->giveUpRegister();
                context->giveUpRegister();

                codeBlock->peekCode<JumpIfUndefinedOrNull>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
            }

            codeBlock->m_shouldClearStack = true;
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            if (property->astNode()->isProperty()) {
                PropertyNode* p = property->astNode()->asProperty();
                if (!(p->key()->isIdentifier() && !p->computed())) {
                    p->key()->iterateChildrenIdentifier(fn);
                }
            }
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            property->astNode()->iterateChildren(fn);
        }
    }

private:
    NodeList m_properties;
};
} // namespace Escargot

#endif
