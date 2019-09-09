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

#ifndef PropertyNode_h
#define PropertyNode_h

#include "Node.h"
#include "StatementNode.h"

namespace Escargot {

class PropertyNode : public Node {
public:
    friend class ScriptParser;
    enum Kind {
        Init,
        Get,
        Set
    };

    PropertyNode(Node* key, Node* value, Kind kind, bool computed, bool shorthand)
        : Node()
        , m_kind(kind)
        , m_computed(computed)
        , m_shorthand(shorthand)
        , m_key(key)
        , m_value(value)
    {
    }

    virtual ~PropertyNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::Property; }
    Node* key()
    {
        return m_key.get();
    }

    Node* value()
    {
        return m_value.get();
    }

    void setValue(Node* value)
    {
        m_value = value;
    }

    Kind kind()
    {
        return m_kind;
    }

    bool computed()
    {
        return m_computed;
    }

    bool isValidPropertyNode()
    {
        return m_key->isIdentifier() || m_key->isLiteral();
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
    {
        ASSERT(m_kind == Init);
        size_t valueIndex = context->getRegister();
        if (m_key->isIdentifier() && !m_computed) {
            AtomicString propertyAtomicName = m_key->asIdentifier()->name();
            codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), srcRegister, valueIndex, propertyAtomicName), context, this);
            m_value->generateResolveAddressByteCode(codeBlock, context);
            m_value->generateStoreByteCode(codeBlock, context, valueIndex, needToReferenceSelf);
        } else {
            size_t propertyIndex = m_key->getRegister(codeBlock, context);
            m_key->generateExpressionByteCode(codeBlock, context, propertyIndex);
            codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), srcRegister, propertyIndex, valueIndex), context, this);
            m_value->generateResolveAddressByteCode(codeBlock, context);
            m_value->generateStoreByteCode(codeBlock, context, valueIndex, needToReferenceSelf);
            context->giveUpRegister(); // for drop propertyIndex
        }
        context->giveUpRegister(); // for drop valueIndex
    }

private:
    Kind m_kind; // kind: "init" | "get" | "set";
    bool m_computed : 1;
    bool m_shorthand : 1;
    RefPtr<Node> m_key; // key: Literal | Identifier;
    RefPtr<Node> m_value; // value: Expression;
};
}

#endif
