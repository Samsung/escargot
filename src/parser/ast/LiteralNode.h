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

#ifndef LiteralNode_h
#define LiteralNode_h

#include "Node.h"
#include "parser/Script.h"

namespace Escargot {

// interface Literal <: Node, Expression {
class LiteralNode : public ExpressionNode {
public:
    explicit LiteralNode(Value value)
        : ExpressionNode()
    {
        m_value = value;
    }

    virtual ASTNodeType type() { return ASTNodeType::Literal; }
    const Value& value() { return m_value; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (m_value.isPointerValue()) {
            codeBlock->m_literalData.pushBack(m_value.asPointerValue());
        }
        if (dstRegister < REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, m_value), context, this);
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t idxExists = SIZE_MAX;
        if (context->m_keepNumberalLiteralsInRegisterFile) {
            if (!m_value.isPointerValue()) {
                for (size_t i = 0; i < context->m_numeralLiteralData->size(); i++) {
                    if ((*context->m_numeralLiteralData)[i] == m_value) {
                        context->pushRegister(REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT + i);
                        return context->getLastRegisterIndex();
                    }
                }
            }
        }
        return context->getRegister();
    }


protected:
    Value m_value;
};
}

#endif
