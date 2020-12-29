/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef ArrowFunctionExpressionNode_h
#define ArrowFunctionExpressionNode_h

#include "FunctionNode.h"

namespace Escargot {

class ArrowParameterPlaceHolderNode : public Node {
public:
    ArrowParameterPlaceHolderNode()
        : Node()
        , m_async(false)
    {
    }

    explicit ArrowParameterPlaceHolderNode(const NodeList& params)
        : Node()
        , m_params(params)
        , m_async(false)
    {
    }

    explicit ArrowParameterPlaceHolderNode(const NodeList& params, bool async)
        : Node()
        , m_params(params)
        , m_async(async)
    {
    }

    NodeList& params()
    {
        return m_params;
    }

    bool async()
    {
        return m_async;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ArrowParameterPlaceHolder; }

private:
    NodeList m_params;
    bool m_async;
};

class ArrowFunctionExpressionNode : public ExpressionNode {
public:
    ArrowFunctionExpressionNode(size_t subCodeBlockIndex)
        : m_subCodeBlockIndex(subCodeBlockIndex - 1)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ArrowFunctionExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        InterpretedCodeBlock* blk = context->m_codeBlock->childBlockAt(m_subCodeBlockIndex);
        if (blk->usesArgumentsObject() && !codeBlock->m_codeBlock->isArrowFunctionExpression()) {
            codeBlock->pushCode(EnsureArgumentsObject(ByteCodeLOC(m_loc.index)), context, this);
        }
        codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, context->m_classInfo.m_thisExpressionIndex, blk), context, this);
    }

private:
    size_t m_subCodeBlockIndex;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
} // namespace Escargot

#endif
