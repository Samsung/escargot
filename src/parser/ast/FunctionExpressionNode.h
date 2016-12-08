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

#ifndef FunctionExpressionNode_h
#define FunctionExpressionNode_h

#include "FunctionNode.h"

namespace Escargot {

class FunctionExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    FunctionExpressionNode(const AtomicString& id, PatternNodeVector&& params, Node* body, ASTScopeContext* scopeContext, bool isGenerator)
        : m_function(id, std::move(params), body, scopeContext, isGenerator, this)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::FunctionExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        CodeBlock* blk = nullptr;
        size_t cnt = 0;
        for (size_t i = 0; i < context->m_codeBlock->childBlocks().size(); i++) {
            CodeBlock* c = context->m_codeBlock->childBlocks()[i];
            if (c->isFunctionExpression()) {
                if (cnt == context->m_feCounter) {
                    blk = c;
                    break;
                }
                cnt++;
            }
        }
        ASSERT(blk);
        codeBlock->pushCode(DeclareFunctionExpression(ByteCodeLOC(m_loc.index), context->getRegister(), blk), context, this);

        context->m_feCounter++;
    }

protected:
    FunctionNode m_function;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
}

#endif
