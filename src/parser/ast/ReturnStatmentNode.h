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

#ifndef ReturnStatmentNode_h
#define ReturnStatmentNode_h

#include "StatementNode.h"

namespace Escargot {

class ReturnStatmentNode : public StatementNode {
public:
    friend class ScriptParser;
    explicit ReturnStatmentNode(Node* argument)
        : StatementNode()
        , m_argument(argument)
    {
    }

    virtual ~ReturnStatmentNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ReturnStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        ASSERT(codeBlock != nullptr);
        ASSERT(context != nullptr);

        if (codeBlock->m_codeBlock->isGenerator() == true) {
            codeBlock->pushCode(GeneratorComplete(ByteCodeLOC(SIZE_MAX)), context, this);
        }

        if (context->m_tryStatementScopeCount != 0) {
            if (m_argument) {
                ByteCodeRegisterIndex index = m_argument->getRegister(codeBlock, context);
                m_argument->generateExpressionByteCode(codeBlock, context, index);
                codeBlock->pushCode(ReturnFunctionSlowCase(ByteCodeLOC(m_loc.index), index), context, this);
                context->giveUpRegister();
            } else {
                codeBlock->pushCode(ReturnFunctionSlowCase(ByteCodeLOC(m_loc.index), REGISTER_LIMIT), context, this);
            }
        } else {
            if (m_argument) {
                ByteCodeRegisterIndex index = m_argument->getRegister(codeBlock, context);
                m_argument->generateExpressionByteCode(codeBlock, context, index);
                codeBlock->pushCode(ReturnFunctionWithValue(ByteCodeLOC(m_loc.index), index), context, this);
                context->giveUpRegister();
            } else {
                if (codeBlock->m_codeBlock->isClassConstructor()) {
                    size_t idx = context->getRegister();
                    codeBlock->pushCode(LoadThisBinding(ByteCodeLOC(SIZE_MAX), idx), context, this);
                    codeBlock->pushCode(ReturnFunctionWithValue(ByteCodeLOC(SIZE_MAX), idx), context, this);
                    context->giveUpRegister();
                } else {
                    codeBlock->pushCode(ReturnFunction(ByteCodeLOC(m_loc.index)), context, this);
                }
            }
        }
    }

private:
    RefPtr<Node> m_argument;
};
}

#endif
