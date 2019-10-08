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

namespace Escargot {

class FunctionExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    FunctionExpressionNode(ASTFunctionScopeContext* scopeContext, bool isGenerator, size_t subCodeBlockIndex)
        : m_isGenerator(isGenerator)
        , m_scopeContext(scopeContext)
        , m_subCodeBlockIndex(subCodeBlockIndex - 1)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::FunctionExpression; }
    ASTFunctionScopeContext* scopeContext() { return m_scopeContext; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        CodeBlock* blk = context->m_codeBlock->asInterpretedCodeBlock()->childBlocks()[m_subCodeBlockIndex];
        if (UNLIKELY(blk->isClassConstructor())) {
            codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), dstIndex, context->m_classInfo.m_prototypeIndex, context->m_classInfo.m_superIndex, blk, context->m_classInfo.m_src), context, this);
        } else if (UNLIKELY(blk->isClassMethod() || blk->isClassStaticMethod())) {
            size_t homeObjectIndex = blk->isClassStaticMethod() ? context->m_classInfo.m_constructorIndex : context->m_classInfo.m_prototypeIndex;
            codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, homeObjectIndex, blk), context, this);
        } else {
            codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, SIZE_MAX, blk), context, this);
        }
    }

    bool isGenerator() const
    {
        return m_isGenerator;
    }

private:
    bool m_isGenerator;
    ASTFunctionScopeContext* m_scopeContext;
    size_t m_subCodeBlockIndex;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
}

#endif
