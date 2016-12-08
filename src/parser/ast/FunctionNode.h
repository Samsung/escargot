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

#ifndef FunctionNode_h
#define FunctionNode_h

#include "ExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class FunctionNode {
public:
    FunctionNode(const AtomicString& id, PatternNodeVector&& params, Node* body, ASTScopeContext* scopeContext, bool isGenerator, Node* node)
    {
        m_id = id;
        m_params = params;
        m_body = body;
        m_isGenerator = isGenerator;
        m_scopeContext = scopeContext;
        m_scopeContext->m_associateNode = node;
    }

    inline const PatternNodeVector& params() { return m_params; }
    inline Node* body() { return m_body; }
    inline const AtomicString& id() { return m_id; }
    ASTScopeContext* scopeContext() { return m_scopeContext; }
protected:
    bool m_isGenerator;
    AtomicString m_id; // id: Identifier;
    PatternNodeVector m_params; // params: [ Pattern ];
    Node* m_body;
    ASTScopeContext* m_scopeContext;
};
}

#endif
