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

#ifndef ArrowFunctionExpressionNode_h
#define ArrowFunctionExpressionNode_h

#include "FunctionNode.h"

namespace Escargot {

class ArrowParameterPlaceHolderNode : public Node {
public:
    ArrowParameterPlaceHolderNode()
        : Node()
    {
    }

    explicit ArrowParameterPlaceHolderNode(ExpressionNodeVector&& params)
        : Node()
        , m_params(std::move(params))
    {
    }

    ExpressionNodeVector& params()
    {
        return m_params;
    }

    virtual ASTNodeType type() { return ASTNodeType::ArrowParameterPlaceHolder; }
private:
    ExpressionNodeVector m_params;
};

class ArrowFunctionExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ArrowFunctionExpressionNode(PatternNodeVector&& params, Node* body, ASTFunctionScopeContext* scopeContext, bool expression)
        : m_function(AtomicString(), std::move(params), body, scopeContext, false, this)
        , m_expression(expression)
    {
        scopeContext->m_isArrowFunctionExpression = true;
    }

    FunctionNode& function()
    {
        return m_function;
    }

    virtual ASTNodeType type() { return ASTNodeType::ArrowFunctionExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex)
    {
        CodeBlock* blk = nullptr;
        size_t cnt = 0;
        for (size_t i = 0; i < context->m_codeBlock->asInterpretedCodeBlock()->childBlocks().size(); i++) {
            CodeBlock* c = context->m_codeBlock->asInterpretedCodeBlock()->childBlocks()[i];
            if (c->isFunctionExpression()) {
                if (cnt == context->m_feCounter) {
                    blk = c;
                    break;
                }
                cnt++;
            }
        }
        ASSERT(blk);
        if (context->m_isWithScope && !context->m_isEvalCode)
            blk->setInWithScope();
        codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, blk), context, this);
        context->m_feCounter++;
    }

private:
    FunctionNode m_function;
    bool m_expression : 1;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
}

#endif
