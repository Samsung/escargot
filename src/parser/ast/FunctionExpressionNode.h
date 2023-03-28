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

#ifndef FunctionExpressionNode_h
#define FunctionExpressionNode_h

namespace Escargot {

class FunctionExpressionNode : public ExpressionNode {
public:
    FunctionExpressionNode(size_t subCodeBlockIndex, const AtomicString& functionName)
        : m_subCodeBlockIndex(subCodeBlockIndex - 1)
        , m_functionName(functionName)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::FunctionExpression; }
    AtomicString functionName() { return m_functionName; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex) override
    {
        InterpretedCodeBlock* blk = context->m_codeBlock->childBlockAt(m_subCodeBlockIndex);
        if (UNLIKELY(blk->isClassConstructor())) {
            codeBlock->pushCode(InitializeClass(ByteCodeLOC(m_loc.index), dstIndex, context->m_classInfo.m_prototypeIndex,
                                                context->m_classInfo.m_superIndex, blk, context->m_classInfo.m_src,
                                                context->m_classInfo.m_name),
                                context, this->m_loc.index);
        } else if (UNLIKELY(blk->isObjectMethod() || blk->isClassMethod() || blk->isClassStaticMethod())) {
            size_t homeObjectIndex = blk->isClassStaticMethod() ? context->m_classInfo.m_constructorIndex : context->m_classInfo.m_prototypeIndex;
            codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, homeObjectIndex, blk), context, this->m_loc.index);
        } else {
            codeBlock->pushCode(CreateFunction(ByteCodeLOC(m_loc.index), dstIndex, REGULAR_REGISTER_LIMIT, blk), context, this->m_loc.index);
        }

        codeBlock->m_shouldClearStack = true;
    }

private:
    size_t m_subCodeBlockIndex;
    AtomicString m_functionName;
    // defaults: [ Expression ];
    // rest: Identifier | null;
};
} // namespace Escargot

#endif
