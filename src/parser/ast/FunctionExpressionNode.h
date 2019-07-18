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

#ifndef FunctionExpressionNode_h
#define FunctionExpressionNode_h

#include "FunctionNode.h"

namespace Escargot {

class FunctionExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    FunctionExpressionNode(const AtomicString& id, PatternNodeVector&& params, Node* body, ASTFunctionScopeContext* scopeContext, bool isGenerator)
        : m_function(id, std::move(params), body, scopeContext, isGenerator, this)
    {
    }

    FunctionNode& function()
    {
        return m_function;
    }

    virtual ASTNodeType type() { return ASTNodeType::FunctionExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex)
    {
        CodeBlock* blk = nullptr;
        size_t cnt = 0;
        const auto& childBlocks = context->m_codeBlock->asInterpretedCodeBlock()->childBlocks();
        size_t len = childBlocks.size();
        size_t counter = context->m_feCounter;
        for (size_t i = 0; i < len; i++) {
            CodeBlock* c = childBlocks[i];
            if (c->isFunctionExpression()) {
                if (cnt == counter) {
                    blk = c;
                    break;
                }
                cnt++;
            }
        }
        ASSERT(blk);
        if (context->m_isWithScope && !context->m_isEvalCode)
            blk->setInWithScope();

        if (UNLIKELY(blk->isClassConstructor())) {
            codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), dstIndex, context->m_classInfo.m_bodyIndex, context->m_classInfo.m_superIndex, context->m_classInfo.m_name, blk, 2), context, this);
        } else {
            codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, blk), context, this);
        }
        context->m_feCounter++;
    }

private:
    FunctionNode m_function;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
}

#endif
