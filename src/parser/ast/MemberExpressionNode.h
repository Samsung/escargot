/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef MemberExpressionNode_h
#define MemberExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"

namespace Escargot {

class MemberExpressionNode : public ExpressionNode {
public:
    MemberExpressionNode(Node* object, Node* property, bool computed)
        : ExpressionNode()
    {
        m_object = object;
        m_property = property;
        m_computed = computed;
    }

    virtual ASTNodeType type() { return ASTNodeType::MemberExpression; }

    bool isPreComputedCase()
    {
        if (!m_computed) {
            ASSERT(m_property->type() == ASTNodeType::Identifier);
            return true;
        } else {
            return false;
        }
    }

    AtomicString propertyName()
    {
        ASSERT(isPreComputedCase());
        return ((IdentifierNode *)m_property)->name();
    }

protected:
    Node* m_object; // object: Expression;
    Node* m_property; // property: Identifier | Expression;

    bool m_computed;
};

}

#endif
