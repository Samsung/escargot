/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ObjectPatternNode_h
#define ObjectPatternNode_h

#include "Node.h"
#include "PropertyNode.h"

namespace Escargot {

class ObjectPatternNode : public Node {
public:
    ObjectPatternNode(const NodeList& properties, bool hasRestElement)
        : m_properties(properties)
        , m_hasRestElement(hasRestElement)
    {
#ifndef NDEBUG
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            if (m_hasRestElement) {
                ASSERT(property->astNode()->isProperty() || property->astNode()->type() == ASTNodeType::RestElement);
            } else {
                ASSERT(property->astNode()->isProperty());
            }
        }
#endif
    }

    ObjectPatternNode(const NodeList& properties, NodeLOC& loc)
        : m_properties(properties)
        , m_hasRestElement(false)
    {
        m_loc = loc;
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            ASSERT(property->astNode()->isProperty() || property->astNode()->type() == ASTNodeType::RestElement);
            if (property->astNode()->type() == ASTNodeType::RestElement) {
                m_hasRestElement = true;
            }
        }
    }

    virtual ASTNodeType type() override { return ASTNodeType::ObjectPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;

        // set m_isLexicallyDeclaredBindingInitialization to false for the case of empty object pattern
        context->m_isLexicallyDeclaredBindingInitialization = false;
        if (m_properties.size() > 0) {
            if (m_hasRestElement) {
                context->m_inObjectDestruction = true;
                ByteCodeRegisterIndex enumDataIndex = context->getRegister();
                codeBlock->pushCode(CreateEnumerateObject(ByteCodeLOC(m_loc.index), srcRegister, enumDataIndex, true), context, this);

                for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
                    context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                    if (LIKELY(property->astNode()->isProperty())) {
                        property->astNode()->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
                    } else {
                        ASSERT(property->astNode()->type() == ASTNodeType::RestElement);
                        property->astNode()->generateStoreByteCode(codeBlock, context, enumDataIndex, needToReferenceSelf);
                    }
                }

                context->giveUpRegister(); // for drop enumDataIndex
                context->m_inObjectDestruction = false;
            } else {
                for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
                    ASSERT(property->astNode()->isProperty());
                    context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
                    property->astNode()->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
                }
            }
        } else {
            // ObjectAssignmentPattern without AssignmentPropertyList requires object-coercible
            // check if srcRegister is undefined or null
            codeBlock->pushCode<JumpIfUndefinedOrNull>(JumpIfUndefinedOrNull(ByteCodeLOC(m_loc.index), true, srcRegister), context, this);
            size_t pos = codeBlock->lastCodePosition<JumpIfUndefinedOrNull>();

            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, ErrorObject::Messages::Can_Not_Be_Destructed), context, this);

            codeBlock->peekCode<JumpIfUndefinedOrNull>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
        }
        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

        codeBlock->m_shouldClearStack = true;
    }

    // FIXME implement iterateChildrenIdentifier in PropertyNode itself
    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* property = m_properties.begin(); property != m_properties.end(); property = property->next()) {
            if (property->astNode()->isProperty()) {
                PropertyNode* p = property->astNode()->asProperty();
                if (!(p->key()->isIdentifier() && !p->computed())) {
                    p->key()->iterateChildrenIdentifier(fn);
                }
            } else {
                property->astNode()->iterateChildrenIdentifier(fn);
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
    bool m_hasRestElement : 1;
};
}

#endif
