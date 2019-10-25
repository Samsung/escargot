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
    ObjectPatternNode(NodeList& properties)
        : m_properties(properties)
    {
#ifndef NDEBUG
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            ASSERT(property->astNode()->isProperty());
        }
#endif
    }

    ObjectPatternNode(NodeList& properties, NodeLOC& loc)
        : m_properties(properties)
    {
        m_loc = loc;
#ifndef NDEBUG
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            ASSERT(property->astNode()->isProperty());
        }
#endif
    }

    virtual ASTNodeType type() override { return ASTNodeType::ObjectPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        if (m_properties.size() > 0) {
            for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
                context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                property->astNode()->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
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
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            PropertyNode* p = property->astNode()->asProperty();
            if (!(p->key()->isIdentifier() && !p->computed())) {
                p->key()->iterateChildrenIdentifier(fn);
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
