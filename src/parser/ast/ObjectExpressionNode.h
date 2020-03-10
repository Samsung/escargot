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

    virtual ASTNodeType type() override { return ASTNodeType::ObjectExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), dstRegister), context, this);
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

                size_t valueIndex = p->value()->getRegister(codeBlock, context);
                const ClassContextInformation classInfoBefore = context->m_classInfo;
                context->m_classInfo.m_prototypeIndex = dstRegister;
                context->m_classInfo.m_constructorIndex = SIZE_MAX;
                context->m_classInfo.m_superIndex = SIZE_MAX;
                p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);
                context->m_classInfo = classInfoBefore;

                if (p->kind() == PropertyNode::Kind::Init) {
                    if (hasKeyName) {
                        codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), objIndex, p->key()->asIdentifier()->name(), valueIndex, ObjectPropertyDescriptor::AllPresent), context, this);
                    } else {
                        bool hasFunctionOnRightSide = p->value()->type() == ASTNodeType::FunctionExpression || p->value()->type() == ASTNodeType::ArrowFunctionExpression;
                        bool hasClassOnRightSide = p->value()->type() == ASTNodeType::ClassExpression && !p->value()->asClassExpression()->classNode().classBody()->hasStaticMemberName(codeBlock->m_codeBlock->context()->staticStrings().name);
                        codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, ObjectPropertyDescriptor::AllPresent, hasFunctionOnRightSide | hasClassOnRightSide), context, this);
                    }
                } else if (p->kind() == PropertyNode::Kind::Get) {
                    codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent), true), context, this);
                } else {
                    ASSERT(p->kind() == PropertyNode::Kind::Set);
                    codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::EnumerablePresent), false), context, this);
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

                size_t cmpIndex = context->getRegister();
                LiteralNode* undefinedNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value());
                size_t undefinedIndex = undefinedNode->getRegister(codeBlock, context);
                undefinedNode->generateExpressionByteCode(codeBlock, context, undefinedIndex);
                codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), elementIndex, undefinedIndex, cmpIndex), context, this);
                codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
                size_t pos1 = codeBlock->lastCodePosition<JumpIfTrue>();
                context->giveUpRegister(); // for drop undefinedIndex

                LiteralNode* nullNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value(Value::Null));
                size_t nullIndex = nullNode->getRegister(codeBlock, context);
                nullNode->generateExpressionByteCode(codeBlock, context, nullIndex);
                codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), elementIndex, nullIndex, cmpIndex), context, this);
                codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
                size_t pos2 = codeBlock->lastCodePosition<JumpIfFalse>();
                context->giveUpRegister(); // for drop nullIndex
                context->giveUpRegister(); // for drop cmpIndex


                codeBlock->pushCode(CreateEnumerateObject(ByteCodeLOC(m_loc.index), elementIndex, dataIndex, true), context, this);

                size_t checkPos = codeBlock->currentCodeSize();
                codeBlock->pushCode(CheckLastEnumerateKey(ByteCodeLOC(m_loc.index)), context, this);
                codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_registerIndex = dataIndex;
                codeBlock->pushCode(GetEnumerateKey(ByteCodeLOC(m_loc.index), keyIndex, dataIndex), context, this);

                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), elementIndex, keyIndex, valueIndex), context, this);
                codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, keyIndex, valueIndex, ObjectPropertyDescriptor::AllPresent, false), context, this);
                codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), checkPos), context, this);

                size_t exitPos = codeBlock->currentCodeSize();
                codeBlock->peekCode<CheckLastEnumerateKey>(checkPos)->m_exitPosition = exitPos;

                // for drop elementIndex, dataIndex, keyIndex, valueIndex
                context->giveUpRegister();
                context->giveUpRegister();
                context->giveUpRegister();
                context->giveUpRegister();

                codeBlock->peekCode<JumpIfTrue>(pos1)->m_jumpPosition = codeBlock->currentCodeSize();
                codeBlock->peekCode<JumpIfTrue>(pos2)->m_jumpPosition = codeBlock->currentCodeSize();
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
}

#endif
