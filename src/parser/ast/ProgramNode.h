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

#ifndef ProgramNode_h
#define ProgramNode_h

#include "Node.h"
#include "StatementNode.h"
#include "parser/ScriptParser.h"
#include "parser/Script.h"

namespace Escargot {

class ProgramNode : public StatementNode {
public:
    ProgramNode(StatementContainer* body, ASTFunctionScopeContext* scopeContext, Script::ModuleData* moduleData, NumeralLiteralVector&& numeralLiteralVector)
        : StatementNode()
        , m_container(body)
        , m_scopeContext(scopeContext)
        , m_moduleData(moduleData)
        , m_numeralLiteralVector(std::move(numeralLiteralVector))
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::Program; }
    ASTFunctionScopeContext* scopeContext() { return m_scopeContext; }
    Script::ModuleData* moduleData() { return m_moduleData; }
    NumeralLiteralVector& numeralLiteralVector() { return m_numeralLiteralVector; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(0);
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext = codeBlock->pushLexicalBlock(context, bi, this);

        size_t start = codeBlock->currentCodeSize();
        m_container->generateStatementByteCode(codeBlock, context);

        codeBlock->finalizeLexicalBlock(context, blockContext);

#ifdef ESCARGOT_DEBUGGER
        if (context->m_breakpointContext->m_breakpointLocations.size() == 0) {
            if (codeBlock->m_isEvalMode) {
                context->insertBreakpointAt(1, this);
            } else {
                insertBreakpoint(context);
            }
        }
#endif /* ESCARGOT_DEBUGGER */
        codeBlock->pushCode(End(ByteCodeLOC(SIZE_MAX), 0), context, this);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_container->iterateChildren(fn);
    }

private:
    StatementContainer* m_container;
    ASTFunctionScopeContext* m_scopeContext;
    Script::ModuleData* m_moduleData;
    NumeralLiteralVector m_numeralLiteralVector;
};
}

#endif
