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

#ifndef WithStatementNode_h
#define WithStatementNode_h

#include "StatementNode.h"

namespace Escargot {

class WithStatementNode : public StatementNode {
public:
    WithStatementNode(Node* object, Node* body)
        : StatementNode()
        , m_object(object)
        , m_body(body)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::WithStatement; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
#ifdef ESCARGOT_DEBUGGER
        insertBreakpoint(context);
#endif /* ESCARGOT_DEBUGGER */

        bool shouldCareScriptExecutionResult = context->shouldCareScriptExecutionResult();
        if (shouldCareScriptExecutionResult) {
            // WithStatement : with ( Expression ) Statement
            // [...]
            // 8. Let C be the result of evaluating Statement.
            // 9. Set the running execution context’s Lexical Environment to oldEnv.
            // 10. If C.[[type]] is normal and C.[[value]] is empty, return
            //     NormalCompletion(undefined).
            // 11. Return Completion(C).
            codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), 0, Value()), context, this);
        }

        size_t start = codeBlock->currentCodeSize();
        if (shouldCareScriptExecutionResult) {
            context->getRegister();
        }
        auto r = m_object->getRegister(codeBlock, context);
        m_object->generateExpressionByteCode(codeBlock, context, r);
        size_t withPos = codeBlock->currentCodeSize();
        context->m_recursiveStatementStack.push_back(std::make_pair(ByteCodeGenerateContext::OpenEnv, withPos));
        if (shouldCareScriptExecutionResult) {
            context->giveUpRegister();
        }
        codeBlock->pushCode(OpenLexicalEnvironment(ByteCodeLOC(m_loc.index), OpenLexicalEnvironment::WithStatement, r), context, this);
        context->giveUpRegister();

        bool isWithScopeBefore = context->m_isWithScope;
        context->m_isWithScope = true;
        m_body->generateStatementByteCode(codeBlock, context);
        context->registerJumpPositionsToComplexCase(start);

        codeBlock->pushCode(CloseLexicalEnvironment(ByteCodeLOC(m_loc.index)), context, this);
        codeBlock->peekCode<OpenLexicalEnvironment>(withPos)->m_endPostion = codeBlock->currentCodeSize();
        context->m_isWithScope = isWithScopeBefore;

        context->m_recursiveStatementStack.pop_back();
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_object->iterateChildren(fn);
        m_body->iterateChildren(fn);
    }

private:
    Node* m_object;
    Node* m_body; // body: [ Statement ];
};
} // namespace Escargot

#endif
