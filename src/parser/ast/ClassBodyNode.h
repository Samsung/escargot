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

#ifndef ClassBody_h
#define ClassBody_h

#include "Node.h"
#include "ClassElementNode.h"
#include "FunctionExpressionNode.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

typedef std::vector<RefPtr<ClassElementNode>> ClassElementNodeVector;

class ClassBodyNode : public Node {
public:
    friend class ScriptParser;
    ClassBodyNode(ClassElementNodeVector&& elementList, RefPtr<FunctionExpressionNode> constructor)
        : Node()
        , m_elementList(elementList)
        , m_constructor(constructor)
    {
    }

    bool hasConstructor()
    {
        return m_constructor != nullptr;
    }

    Node* constructor()
    {
        return m_constructor.get();
    }

    void generateClassInitializer(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex classIndex)
    {
        size_t objIndex = context->m_classInfo.m_prototypeIndex;

        for (unsigned i = 0; i < m_elementList.size(); i++) {
            ClassElementNode* p = m_elementList[i].get();

            size_t destIndex = p->isStatic() ? classIndex : objIndex;

            AtomicString propertyAtomicName;
            bool hasKey = false;
            size_t propertyIndex = SIZE_MAX;
            if (p->key()->isIdentifier() && !p->isComputed()) {
                if (p->kind() == ClassElementNode::Kind::Method) {
                    propertyAtomicName = p->key()->asIdentifier()->name();
                    hasKey = true;
                } else {
                    propertyIndex = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), propertyIndex, Value(p->key()->asIdentifier()->name().string())), context, this);
                }
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

                codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_Class_Prototype_Is_Not_Static_Generator), context, this);

                codeBlock->peekCode<JumpIfFalse>(jmpPos)->m_jumpPosition = codeBlock->currentCodeSize();

                context->giveUpRegister();
                context->giveUpRegister();
            }

            size_t valueIndex = p->value()->getRegister(codeBlock, context);
            p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);

            if (p->kind() == ClassElementNode::Kind::Method) {
                if (hasKey) {
                    codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), destIndex, propertyAtomicName, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)), context, this);
                } else {
                    codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)), context, this);
                    context->giveUpRegister(); // for drop property index
                }
            } else if (p->kind() == ClassElementNode::Kind::Get) {
                codeBlock->pushCode(ObjectDefineGetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex), context, this);
                context->giveUpRegister(); // for drop property index
            } else {
                ASSERT(p->kind() == ClassElementNode::Kind::Set);
                codeBlock->pushCode(ObjectDefineSetter(ByteCodeLOC(m_loc.index), destIndex, propertyIndex, valueIndex), context, this);
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

        for (unsigned i = 0; i < m_elementList.size(); i++) {
            ClassElementNode* p = m_elementList[i].get();
            p->iterateChildren(fn);
        }
    }

private:
    ClassElementNodeVector m_elementList;
    RefPtr<FunctionExpressionNode> m_constructor;
};
}

#endif
