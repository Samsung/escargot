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

#ifndef RegExpLiteralNode_h
#define RegExpLiteralNode_h

#include "Node.h"

namespace Escargot {

// interface RegExpLiteral <: Node, Expression {
class RegExpLiteralNode : public Node {
public:
    RegExpLiteralNode(String* body, String* flag)
        : Node()
    {
        m_body = body;
        m_flag = flag;
    }

    virtual ASTNodeType type() { return ASTNodeType::RegExpLiteral; }
    String* body() { return m_body; }
    String* flag() { return m_flag; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        codeBlock->m_literalData.pushBack(m_body);
        codeBlock->m_literalData.pushBack(m_flag);
        codeBlock->pushCode(LoadRegexp(ByteCodeLOC(m_loc.index), dstRegister, m_body, m_flag), context, this);
    }

protected:
    String* m_body;
    String* m_flag;
};
}

#endif
