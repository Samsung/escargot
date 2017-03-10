/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef VariableDeclarationNode_h
#define VariableDeclarationNode_h

#include "DeclarationNode.h"
#include "VariableDeclaratorNode.h"

namespace Escargot {

class VariableDeclarationNode : public DeclarationNode {
public:
    friend class ScriptParser;
    VariableDeclarationNode(VariableDeclaratorVector&& decl)
        : DeclarationNode()
    {
        m_declarations = decl;
    }

    virtual ~VariableDeclarationNode()
    {
        size_t len = m_declarations.size();
        for (size_t i = 0; i < len; i++) {
            delete m_declarations[i];
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::VariableDeclaration; }
    VariableDeclaratorVector& declarations() { return m_declarations; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        size_t len = m_declarations.size();
        for (size_t i = 0; i < len; i++) {
            m_declarations[i]->generateStatementByteCode(codeBlock, context);
        }
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex src, bool needToReferenceSelf = true)
    {
        ASSERT(m_declarations.size() == 1);
        if (m_declarations[0]->init()) {
            src = m_declarations[0]->init()->getRegister(codeBlock, context);
            m_declarations[0]->init()->generateExpressionByteCode(codeBlock, context, src);
            context->giveUpRegister();
        }
        m_declarations[0]->id()->generateStoreByteCode(codeBlock, context, src, false);
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
    }

protected:
    VariableDeclaratorVector m_declarations; // declarations: [ VariableDeclarator ];
    // kind: "var" | "let" | "const";
};
}

#endif
