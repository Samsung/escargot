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

#ifndef ReturnStatementNode_h
#define ReturnStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class ReturnStatementNode : public StatementNode {
public:
    explicit ReturnStatementNode(Node* argument)
        : StatementNode()
        , m_argument(argument)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ReturnStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        if (codeBlock->codeBlock()->markDebugging()) {
            InterpretedCodeBlock* interpretedCodeBlock = context->m_codeBlock;
            if (interpretedCodeBlock->hasRareData() && interpretedCodeBlock->rareData()->m_debuggerLineStart != SIZE_MAX) {
                ASSERT(interpretedCodeBlock->isOneExpressionOnlyVirtualArrowFunctionExpression());
                context->insertBreakpointAt(interpretedCodeBlock->rareData()->m_debuggerLineStart, this);
            } else {
                insertBreakpoint(context);
            }
        }
#endif /* ESCARGOT_DEBUGGER */

        if (context->tryCatchWithBlockStatementCount() != 0) {
            ByteCodeRegisterIndex index;
            if (m_argument) {
                index = m_argument->getRegister(codeBlock, context);
                m_argument->generateExpressionByteCode(codeBlock, context, index);
                codeBlock->pushCode(ReturnFunctionSlowCase(ByteCodeLOC(m_loc.index), index), context, this);
            } else {
                index = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), index, Value()), context, this);
                codeBlock->pushCode(ReturnFunctionSlowCase(ByteCodeLOC(m_loc.index), index), context, this);
            }
            context->giveUpRegister();
        } else {
            ByteCodeRegisterIndex r;
            if (m_argument) {
                r = m_argument->getRegister(codeBlock, context);
                // consider TCO
                m_argument->generateTCOExpressionByteCode(codeBlock, context, r);
            } else {
                r = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), r, Value()), context, this);
            }
            codeBlock->pushCode(End(ByteCodeLOC(m_loc.index), r), context, this);
            context->giveUpRegister();
        }
    }

    static void generateReturnCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, Node* srcNode, ByteCodeLOC loc, size_t srcRegister)
    {
        if (context->tryCatchWithBlockStatementCount() != 0) {
            codeBlock->pushCode(ReturnFunctionSlowCase(loc, srcRegister), context, srcNode);
        } else {
            codeBlock->pushCode(End(loc, srcRegister), context, srcNode);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_argument) {
            m_argument->iterateChildren(fn);
        }
    }

private:
    Node* m_argument;
};
} // namespace Escargot

#endif
