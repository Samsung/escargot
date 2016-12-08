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

#ifndef FunctionDeclarationNode_h
#define FunctionDeclarationNode_h

#include "FunctionNode.h"

namespace Escargot {

class FunctionDeclarationNode : public StatementNode {
public:
    friend class ScriptParser;
    FunctionDeclarationNode(const AtomicString& id, PatternNodeVector&& params, Node* body, ASTScopeContext* scopeContext, bool isGenerator)
        : m_function(id, std::move(params), body, scopeContext, isGenerator, this)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::FunctionDeclaration; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        // do nothing
    }

protected:
    FunctionNode m_function;
};
}

#endif
