/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ObjectPatternNode_h
#define ObjectPatternNode_h

#include "Node.h"
#include "PropertyNode.h"

namespace Escargot {

class ObjectPatternNode : public Node {
public:
    friend class ScriptParser;
    ObjectPatternNode(PropertiesNodeVector&& properties)
        : m_properties(properties)
    {
#ifndef NDEBUG
        for (size_t i = 0; i < m_properties.size(); i++) {
            ASSERT(m_properties[i]->isProperty());
        }
#endif
    }

    ObjectPatternNode(PropertiesNodeVector&& properties, NodeLOC& loc)
        : m_properties(properties)
    {
        m_loc = loc;
#ifndef NDEBUG
        for (size_t i = 0; i < m_properties.size(); i++) {
            ASSERT(m_properties[i]->isProperty());
        }
#endif
    }

    virtual ASTNodeType type() override { return ASTNodeType::ObjectPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        if (m_properties.size() > 0) {
            for (size_t i = 0; i < m_properties.size(); i++) {
                context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                m_properties[i]->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
            }
        } else {
            // ObjectAssignmentPattern without AssignmentPropertyList requires object-coercible
            // check if srcRegister is undefined or null
            size_t cmpIndex = context->getRegister();

            LiteralNode* undefinedNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value());
            size_t undefinedIndex = undefinedNode->getRegister(codeBlock, context);
            undefinedNode->generateExpressionByteCode(codeBlock, context, undefinedIndex);
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), srcRegister, undefinedIndex, cmpIndex), context, this);
            codeBlock->pushCode<JumpIfTrue>(JumpIfTrue(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
            size_t pos1 = codeBlock->lastCodePosition<JumpIfTrue>();
            context->giveUpRegister(); // for drop undefinedIndex

            LiteralNode* nullNode = new (alloca(sizeof(LiteralNode))) LiteralNode(Value(Value::Null));
            size_t nullIndex = nullNode->getRegister(codeBlock, context);
            nullNode->generateExpressionByteCode(codeBlock, context, nullIndex);
            codeBlock->pushCode(BinaryEqual(ByteCodeLOC(m_loc.index), srcRegister, nullIndex, cmpIndex), context, this);
            codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
            size_t pos2 = codeBlock->lastCodePosition<JumpIfFalse>();
            context->giveUpRegister(); // for drop nullIndex
            context->giveUpRegister(); // for drop cmpIndex

            codeBlock->peekCode<JumpIfTrue>(pos1)->m_jumpPosition = codeBlock->currentCodeSize();
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_Can_Not_Be_Destructed), context, this);
            codeBlock->peekCode<JumpIfTrue>(pos2)->m_jumpPosition = codeBlock->currentCodeSize();
        }
        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
    }

    // FIXME implement iterateChildrenIdentifier in PropertyNode itself
    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (size_t i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i].get()->asProperty();
            if (!(p->key()->isIdentifier() && !p->computed())) {
                p->key()->iterateChildrenIdentifier(fn);
            }
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (size_t i = 0; i < m_properties.size(); i++) {
            m_properties[i]->iterateChildren(fn);
        }
    }

private:
    PropertiesNodeVector m_properties;
};
}

#endif
