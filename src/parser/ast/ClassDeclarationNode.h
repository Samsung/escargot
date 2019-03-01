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

#ifndef ClassDeclarationNode_h
#define ClassDeclarationNode_h

#include "ClassNode.h"
#include "FunctionExpressionNode.h"
#include "StatementNode.h"

namespace Escargot {

class ClassDeclarationNode : public StatementNode {
public:
    friend class ScriptParser;
    ClassDeclarationNode(RefPtr<IdentifierNode> id, RefPtr<Node> superClass, RefPtr<ClassBodyNode> classBody, Context* ctx)
        : StatementNode()
        , m_class(id, superClass, classBody)
        , m_ctx(ctx)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ClassMethod; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->getRegister(); // To ensure that the result of the classDeclaration is undefined
        size_t classIndex = context->getRegister();
        RefPtr<IdentifierNode> classIdent = m_class.id();

        if (m_class.classBody()->hasConstructor()) {
            Node* constructor = m_class.classBody()->constructor();
            RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(classIdent.get(), constructor));
            assign->m_loc = m_loc;
            assign->generateExpressionByteCode(codeBlock, context, classIndex);
        } else {
            codeBlock->pushCode(CreateImplicitConstructor(ByteCodeLOC(m_loc.index), classIndex), context, this);
            classIdent->generateResolveAddressByteCode(codeBlock, context);
            classIdent->generateStoreByteCode(codeBlock, context, classIndex, false);
        }

        m_class.classBody()->generateClassInitializer(codeBlock, context, classIndex, m_ctx);

        context->giveUpRegister(); // for drop classIndex
        context->giveUpRegister();
        codeBlock->m_shouldClearStack = true;
    }

private:
    ClassNode m_class;
    Context* m_ctx;
};
}

#endif
