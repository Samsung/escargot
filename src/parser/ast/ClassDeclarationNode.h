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
    ClassDeclarationNode(RefPtr<IdentifierNode> id, RefPtr<Node> superClass, RefPtr<ClassBodyNode> classBody)
        : StatementNode()
        , m_class(id, superClass, classBody)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ClassMethod; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->getRegister(); // To ensure that the result of the classDeclaration is undefined
        size_t classIndex = context->getRegister();
        RefPtr<IdentifierNode> classIdent = m_class.id();

        const ClassContextInformation classInfoBefore = context->m_classInfo;
        context->m_classInfo.m_bodyIndex = context->getRegister();
        context->m_classInfo.m_superIndex = m_class.superClass() ? context->getRegister() : SIZE_MAX;
        context->m_classInfo.m_name = classIdent ? classIdent.get()->name() : AtomicString();

        codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), classIndex, context->m_classInfo.m_bodyIndex, context->m_classInfo.m_superIndex, context->m_classInfo.m_name, nullptr, 1), context, this);

        if (m_class.superClass() != nullptr) {
            m_class.superClass()->generateExpressionByteCode(codeBlock, context, context->m_classInfo.m_superIndex);
        }

        if (m_class.classBody()->hasConstructor()) {
            Node* constructor = m_class.classBody()->constructor();
            RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(classIdent.get(), constructor));
            assign->m_loc = m_loc;
            assign->generateExpressionByteCode(codeBlock, context, classIndex);
        } else {
            codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), classIndex, context->m_classInfo.m_bodyIndex, context->m_classInfo.m_superIndex, context->m_classInfo.m_name, nullptr, 2), context, this);
            classIdent->generateResolveAddressByteCode(codeBlock, context);
            classIdent->generateStoreByteCode(codeBlock, context, classIndex, false);
        }

        m_class.classBody()->generateClassInitializer(codeBlock, context, classIndex);

        codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), classIndex, context->m_classInfo.m_bodyIndex, context->m_classInfo.m_superIndex, context->m_classInfo.m_name, nullptr, 3), context, this);

        if (context->m_classInfo.m_superIndex != SIZE_MAX) {
            context->giveUpRegister(); // for drop m_superIndex
        }
        context->giveUpRegister(); // for drop m_bodyIndex
        context->giveUpRegister(); // for drop classIndex
        context->giveUpRegister(); // To ensure that the result of the classDeclaration is undefined
        codeBlock->m_shouldClearStack = true;

        context->m_classInfo = classInfoBefore;
    }

private:
    ClassNode m_class;
};
}

#endif
