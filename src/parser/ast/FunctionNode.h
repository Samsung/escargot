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

#ifndef FunctionNode_h
#define FunctionNode_h

#include "Node.h"
#include "StatementNode.h"
#include "BlockStatementNode.h"

namespace Escargot {

class FunctionNode : public StatementNode {
public:
    FunctionNode(StatementContainer* params, BlockStatementNode* body, NumeralLiteralVector&& numeralLiteralVector)
        : StatementNode()
        , m_params(params)
        , m_body(body)
        , m_numeralLiteralVector(std::move(numeralLiteralVector))
    {
    }

    virtual ~FunctionNode()
    {
    }

    NumeralLiteralVector& numeralLiteralVector() { return m_numeralLiteralVector; }
    virtual ASTNodeType type() override { return ASTNodeType::Function; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        m_params->generateStatementByteCode(codeBlock, context);
        m_body->generateStatementByteCode(codeBlock, context);
    }

    BlockStatementNode* body()
    {
        ASSERT(!!m_body);
        return m_body.get();
    }

private:
    RefPtr<StatementContainer> m_params;
    RefPtr<BlockStatementNode> m_body;
    NumeralLiteralVector m_numeralLiteralVector;
};
}

#endif
