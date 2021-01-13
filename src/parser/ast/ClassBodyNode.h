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

        for (SentinelNode* element = m_elementList.begin(); element != m_elementList.end(); element = element->next()) {
            ClassElementNode* p = element->astNode()->asClassElement();

            size_t destIndex = p->isStatic() ? classIndex : objIndex;

            bool hasKeyName = false;
            size_t propertyIndex = SIZE_MAX;
            if (p->key()->isIdentifier() && !p->isComputed()) {
                hasKeyName = true;
            } else {
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

            size_t oldThisIndex = context->m_classInfo.m_thisExpressionIndex;

            size_t valueExprStartPos = codeBlock->currentCodeSize();

            bool needsOpenEnv = p->hasSuperPropertyExpressionOnFieldInitializer() || p->hasFunctionOnFieldInitializer();
            // TODO check using super keyword correctly
            bool needsHeapEnvWhenOpen = p->hasFunctionOnFieldInitializer() || !codeBlock->m_codeBlock->canAllocateEnvironmentOnStack();
            if (p->kind() == ClassElementNode::Kind::StaticField) {
                context->m_classInfo.m_thisExpressionIndex = context->m_classInfo.m_constructorIndex;

                if (needsOpenEnv) {
                    context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::OpenEnv, valueExprStartPos));
                    context->m_openedNonBlockEnvCount++;
                    codeBlock->pushCode(OpenLexicalEnvironment(ByteCodeLOC(m_loc.index),
                                                               needsHeapEnvWhenOpen ? OpenLexicalEnvironment::ClassStaticFieldInitWithHeapEnv : OpenLexicalEnvironment::ClassStaticFieldInit, context->m_classInfo.m_thisExpressionIndex),
                                        context, this);
                }
            }

            size_t valueIndex = p->value()->getRegister(codeBlock, context);
            p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);

            if (p->kind() == ClassElementNode::Kind::StaticField) {
                context->m_classInfo.m_thisExpressionIndex = oldThisIndex;

                if (needsOpenEnv) {
                    codeBlock->pushCode(CloseLexicalEnvironment(ByteCodeLOC(m_loc.index)), context, this);
                    codeBlock->peekCode<OpenLexicalEnvironment>(valueExprStartPos)->m_endPostion = codeBlock->currentCodeSize();
                    context->m_openedNonBlockEnvCount--;
                    context->m_recursiveStatementStack.pop_back();
                }
            }

            if (p->kind() == ClassElementNode::Kind::Method) {
                if (hasKeyName) {
                    codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), destIndex, p->key()->asIdentifier()->name(), valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)), context, this);
                } else {
                    codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent), true), context, this);
                }
            } else if (p->kind() == ClassElementNode::Kind::StaticField) {
                if (hasKeyName) {
                    codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), destIndex, p->key()->asIdentifier()->name(), valueIndex, ObjectPropertyDescriptor::AllPresent), context, this);
                } else {
                    codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, ObjectPropertyDescriptor::AllPresent, false), context, this);
                }
            } else if (p->kind() == ClassElementNode::Kind::Get) {
                codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent), true), context, this);
            } else {
                ASSERT(p->kind() == ClassElementNode::Kind::Set);
                codeBlock->pushCode(ObjectDefineGetterSetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::NonEnumerablePresent), false), context, this);
            }

            if (!hasKeyName) {
                context->giveUpRegister(); // for drop property index
            }

            context->giveUpRegister(); // for drop value index
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
