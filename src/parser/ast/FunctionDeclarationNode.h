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

#ifndef FunctionDeclarationNode_h
#define FunctionDeclarationNode_h

namespace Escargot {

class FunctionDeclarationNode : public StatementNode {
public:
    friend class ScriptParser;
    FunctionDeclarationNode(ASTFunctionScopeContext* scopeContext, bool isGenerator, size_t /*subCodeBlockIndex not used yet*/)
        : m_isGenerator(isGenerator)
    {
        scopeContext->m_nodeType = this->type();
        scopeContext->m_isGenerator = isGenerator;
    }

    virtual ASTNodeType type() override { return ASTNodeType::FunctionDeclaration; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        // do nothing
    }

    bool isGenerator() const
    {
        return m_isGenerator;
    }

private:
    bool m_isGenerator;
};
}

#endif
