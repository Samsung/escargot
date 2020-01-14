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
    ClassDeclarationNode() {}
    ClassDeclarationNode(Node* id, Node* superClass, Node* classBody, LexicalBlockIndex classBodyLexicalBlockIndex, StringView classSrc)
        : StatementNode()
        // id can be nullptr
        , m_class(id, superClass, classBody->asClassBody(), classBodyLexicalBlockIndex, classSrc)
    {
    }

    const ClassNode& classNode()
    {
        return m_class;
    }

    virtual ASTNodeType type() override { return ASTNodeType::ClassDeclaration; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        context->getRegister(); // To ensure that the result of the classDeclaration is undefined
        size_t classIndex = context->getRegister();
        Node* classIdent = m_class.id();

        const ClassContextInformation classInfoBefore = context->m_classInfo;
        context->m_classInfo.m_constructorIndex = classIndex;
        context->m_classInfo.m_prototypeIndex = context->getRegister();
        context->m_classInfo.m_superIndex = m_class.superClass() ? context->getRegister() : SIZE_MAX;

        context->m_classInfo.m_name = classIdent ? classIdent->asIdentifier()->name() : AtomicString();
        context->m_classInfo.m_src = new StringView(m_class.classSrc());
        codeBlock->m_literalData.push_back(context->m_classInfo.m_src);

        size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
        if (m_class.classBodyLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            context->m_lexicalBlockIndex = m_class.classBodyLexicalBlockIndex();
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_class.classBodyLexicalBlockIndex());
            blockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        if (m_class.superClass() != nullptr) {
            m_class.superClass()->generateExpressionByteCode(codeBlock, context, context->m_classInfo.m_superIndex);
        }

        if (m_class.classBody()->hasConstructor()) {
            m_class.classBody()->constructor()->generateExpressionByteCode(codeBlock, context, classIndex);
        } else {
            codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), classIndex, context->m_classInfo.m_prototypeIndex, context->m_classInfo.m_superIndex, nullptr, context->m_classInfo.m_src), context, this);
        }

        m_class.classBody()->generateClassInitializer(codeBlock, context, classIndex);

        size_t nameRegister = context->getRegister();
        // we don't need to root class name string because it is AtomicString
        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), nameRegister, Value(context->m_classInfo.m_name.string())), context, this);
        codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), classIndex,
                                                                     codeBlock->m_codeBlock->context()->staticStrings().name,
                                                                     nameRegister, ObjectPropertyDescriptor::ConfigurablePresent),
                            context, this);
        context->giveUpRegister();

        if (m_class.classBodyLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            ASSERT(classIdent);
            // Initialize class name
            context->m_isLexicallyDeclaredBindingInitialization = true;
            classIdent->generateStoreByteCode(codeBlock, context, classIndex, true);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

            codeBlock->finalizeLexicalBlock(context, blockContext);
            context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
        }
        ASSERT(classIdent);

        context->m_isLexicallyDeclaredBindingInitialization = true;
        classIdent->generateStoreByteCode(codeBlock, context, classIndex, true);
        ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

        if (context->m_classInfo.m_superIndex != SIZE_MAX) {
            context->giveUpRegister(); // for drop m_superIndex
        }
        context->giveUpRegister(); // for drop m_bodyIndex
        context->giveUpRegister(); // for drop classIndex
        context->giveUpRegister(); // To ensure that the result of the classDeclaration is undefined
        codeBlock->m_shouldClearStack = true;

        context->m_classInfo = classInfoBefore;
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        if (m_class.id()) {
            m_class.id()->iterateChildren(fn);
        }

        if (m_class.superClass()) {
            m_class.superClass()->iterateChildren(fn);
        }

        if (m_class.classBody()) {
            m_class.classBody()->iterateChildren(fn);
        }
    }

private:
    ClassNode m_class;
};
}

#endif
