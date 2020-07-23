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

#ifndef RegExpLiteralNode_h
#define RegExpLiteralNode_h

#include "Node.h"

namespace Escargot {

// interface RegExpLiteral <: Node, Expression {
class RegExpLiteralNode : public Node {
public:
    RegExpLiteralNode(String* body, String* flag)
        : Node()
        , m_body(body)
        , m_flag(flag)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::RegExpLiteral; }
    String* body() { return m_body; }
    String* flag() { return m_flag; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        codeBlock->m_stringLiteralData.pushBack(m_body);
        codeBlock->m_stringLiteralData.pushBack(m_flag);
        codeBlock->pushCode(LoadRegexp(ByteCodeLOC(m_loc.index), dstRegister, m_body, m_flag), context, this);
    }

private:
    String* m_body;
    String* m_flag;
};
}

#endif
