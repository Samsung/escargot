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

#ifndef LiteralNode_h
#define LiteralNode_h

#include "Node.h"
#include "parser/Script.h"

namespace Escargot {

// interface Literal <: Node, Expression {
class LiteralNode : public ExpressionNode {
public:
    LiteralNode(Value value)
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
        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, m_value), context, this);
    }

protected:
    Value m_value;
};
}

#endif
