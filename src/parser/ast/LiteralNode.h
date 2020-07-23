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

#ifndef LiteralNode_h
#define LiteralNode_h

#include "Node.h"
#include "parser/Script.h"

namespace Escargot {

// interface Literal <: Node, Expression {
class LiteralNode : public ExpressionNode {
public:
    explicit LiteralNode(const Value& value)
        : ExpressionNode()
        , m_value(value)
    {
        // LiteralNode is allowed to have non-pointer value or string value only
        ASSERT(!value.isPointerValue() || value.asPointerValue()->isString());
    }

    virtual ASTNodeType type() override { return ASTNodeType::Literal; }
    const Value& value() { return m_value; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        if (m_value.isPointerValue()) {
            ASSERT(m_value.asPointerValue()->isString());
            codeBlock->m_stringLiteralData.pushBack(m_value.asPointerValue()->asString());
        }
        if (dstRegister < REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT) {
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, m_value), context, this);
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        size_t idxExists = SIZE_MAX;
        if (context->m_keepNumberalLiteralsInRegisterFile && !m_value.isPointerValue()) {
            for (size_t i = 0; i < context->m_numeralLiteralData->size(); i++) {
                if ((*context->m_numeralLiteralData)[i] == m_value) {
                    context->pushRegister(REGULAR_REGISTER_LIMIT + VARIABLE_LIMIT + i);
                    return context->getLastRegisterIndex();
                }
            }
        }
        return context->getRegister();
    }


private:
    Value m_value;
};
}

#endif
