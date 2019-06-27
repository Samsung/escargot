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

#ifndef FunctionNode_h
#define FunctionNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"
#include "IdentifierNode.h"

namespace Escargot {

class FunctionNode {
public:
    FunctionNode(const AtomicString& id, PatternNodeVector&& params, Node* body, ASTScopeContext* scopeContext, bool isGenerator, Node* node)
        : m_isGenerator(isGenerator)
        , m_id(id)
        , m_params(std::move(params))
        , m_body(body)
        , m_scopeContext(scopeContext)
    {
        m_scopeContext->m_nodeType = node->type();
        m_scopeContext->m_isGenerator = isGenerator;
    }
    ~FunctionNode()
    {
    }

    AtomicStringVector paramsToAtomicStringVector()
    {
        AtomicStringVector ret;

        ret.resizeWithUninitializedValues(m_params.size());
        for (size_t i = 0; i < m_params.size(); i++) {
            ret[i] = m_params[i]->asIdentifier()->name();
        }

        return ret;
    }

    inline const PatternNodeVector& params() { return m_params; }
    inline Node* body() { return m_body; }
    inline const AtomicString& id() { return m_id; }
    ASTScopeContext* scopeContext() { return m_scopeContext; }
    inline bool isGenerator() { return m_isGenerator; };
private:
    bool m_isGenerator : 1;
    AtomicString m_id; // id: Identifier;
    PatternNodeVector m_params; // params: [ Pattern ];
    Node* m_body;
    ASTScopeContext* m_scopeContext;
};
}

#endif
