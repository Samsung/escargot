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

#ifndef ClassBody_h
#define ClassBody_h

#include "Node.h"
#include "ClassElementNode.h"
#include "FunctionExpressionNode.h"
#include "runtime/ErrorObject.h"

namespace Escargot {


class ClassBodyNode : public Node {
public:
    ClassBodyNode(const NodeList& elementList, Node* constructor)
        : Node()
        , m_elementList(elementList)
        , m_constructor(constructor)
    {
    }

    bool hasConstructor()
    {
        return m_constructor != nullptr;
    }

    bool hasStaticMemberName(AtomicString name)
    {
        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();
            if (p->isStatic() && !p->isComputed() && p->key()->isIdentifier() && p->key()->asIdentifier()->name() == name) {
                return true;
            }
        }

        return false;
    }

    Node* constructor()
    {
        return m_constructor;
    }

    void generateClassInitializer(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex classIndex)
    {
        size_t objIndex = context->m_classInfo.m_prototypeIndex;

        size_t fieldSize = 0;
        size_t staticFieldSize = 0;
        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();
            if (p->kind() == ClassElementNode::Kind::Field) {
                fieldSize++;
            } else if (p->kind() == ClassElementNode::Kind::StaticField) {
                staticFieldSize++;
            }
        }

        if (fieldSize || staticFieldSize) {
            codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex, fieldSize, staticFieldSize), context, this);
        }

        size_t fieldIndex = 0;
        size_t staticFieldIndex = 0;
        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();
            bool hasKeyName = p->key()->isIdentifier() && !p->isComputed();
            if (p->kind() == ClassElementNode::Kind::Field) {
                ByteCodeRegisterIndex keyIndex = context->getRegister();
                if (hasKeyName) {
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), keyIndex, p->key()->asIdentifier()->name().string()), context, this);
                } else {
                    p->key()->generateExpressionByteCode(codeBlock, context, keyIndex);
                }

                codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex, fieldIndex, keyIndex, InitializeClass::InitFieldDataTagValue), context, this);

                fieldIndex++;
                context->giveUpRegister();
            } else if (p->kind() == ClassElementNode::Kind::StaticField) {
                ByteCodeRegisterIndex keyIndex = context->getRegister();
                if (hasKeyName) {
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), keyIndex, p->key()->asIdentifier()->name().string()), context, this);
                } else {
                    p->key()->generateExpressionByteCode(codeBlock, context, keyIndex);
                }

                codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex, staticFieldIndex, keyIndex, InitializeClass::InitStaticFieldDataTagValue), context, this);

                staticFieldIndex++;
                context->giveUpRegister();
            }
        }

        fieldIndex = 0;
        staticFieldIndex = 0;

        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();

            if (p->kind() == ClassElementNode::Field) {
                ByteCodeRegisterIndex valueIndex = context->getRegister();

                p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);
                codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex, fieldIndex, valueIndex, InitializeClass::SetFieldDataTagValue), context, this);

                fieldIndex++;
                context->giveUpRegister();
                continue;
            } else if (p->kind() == ClassElementNode::Kind::StaticField) {
                ByteCodeRegisterIndex valueIndex = context->getRegister();

                p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);
                codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex, staticFieldIndex, valueIndex, InitializeClass::SetStaticFieldDataTagValue), context, this);

                staticFieldIndex++;
                context->giveUpRegister();
                continue;
            }

            bool hasKeyName = p->key()->isIdentifier() && !p->isComputed();
            size_t destIndex = p->isStatic() ? classIndex : objIndex;
            size_t propertyIndex = SIZE_MAX;
            if (p->kind() != ClassElementNode::Kind::Field && p->kind() != ClassElementNode::Kind::StaticField) {
                if (!hasKeyName) {
                    propertyIndex = p->key()->getRegister(codeBlock, context);
                    p->key()->generateExpressionByteCode(codeBlock, context, propertyIndex);
                }

                // if computed && key == "prototype" && static throw typeerror
                if (p->isStatic() && p->isComputed()) {
                    // we don't need to root `prototype` string because it is AtomicString
                    auto stringReg = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), stringReg, Value(context->m_codeBlock->context()->staticStrings().prototype.string())), context, this);
                    auto testReg = context->getRegister();
                    codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), propertyIndex, stringReg, testReg), context, this);

                    size_t jmpPos = codeBlock->currentCodeSize();
                    codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this);

                    codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, ErrorObject::Messages::Class_Prototype_Is_Not_Static_Generator), context, this);

                    codeBlock->peekCode<JumpIfFalse>(jmpPos)->m_jumpPosition = codeBlock->currentCodeSize();

                    context->giveUpRegister();
                    context->giveUpRegister();
                }
            }

            size_t valueExprStartPos = codeBlock->currentCodeSize();

            size_t valueIndex = p->value()->getRegister(codeBlock, context);
            p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);

            if (p->kind() == ClassElementNode::Kind::Method) {
                if (hasKeyName) {
                    codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), destIndex, p->key()->asIdentifier()->name(), valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)), context, this);
                } else {
                    codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent), true), context, this);
                }
            } else if (p->kind() == ClassElementNode::Kind::Get) {
                codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent), true), context, this);
            } else {
                ASSERT(p->kind() == ClassElementNode::Kind::Set);
                codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent), false), context, this);
            }

            if (propertyIndex != SIZE_MAX) {
                context->giveUpRegister(); // for drop property index
            }

            context->giveUpRegister(); // for drop value index
        }

        if (staticFieldSize) {
            codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), context->m_classInfo.m_constructorIndex), context, this);
        }

        codeBlock->m_shouldClearStack = true;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ClassBody; }
    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_constructor) {
            m_constructor->iterateChildren(fn);
        }

        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();
            p->iterateChildren(fn);
        }
    }

private:
    NodeList m_elementList;
    Node* m_constructor;
};
} // namespace Escargot

#endif
