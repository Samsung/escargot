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

#ifndef ClassExpressionNode_h
#define ClassExpressionNode_h

#include "ClassNode.h"
#include "ClassBodyNode.h"
#include "FunctionExpressionNode.h"
#include "ExpressionNode.h"

namespace Escargot {

class ClassExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ClassExpressionNode(RefPtr<IdentifierNode> id, RefPtr<Node> superClass, RefPtr<ClassBodyNode> classBody, LexicalBlockIndex classBodyLexicalBlockIndex)
        : ExpressionNode()
        , m_class(id, superClass, classBody, classBodyLexicalBlockIndex)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ClassExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex)
    {
        RefPtr<IdentifierNode> classIdent = m_class.id();

        const ClassContextInformation classInfoBefore = context->m_classInfo;
        context->m_classInfo.m_bodyIndex = context->getRegister();
        context->m_classInfo.m_superIndex = m_class.superClass() ? context->getRegister() : SIZE_MAX;
        context->m_classInfo.m_name = classIdent ? classIdent.get()->name() : AtomicString();

        size_t lexicalBlockIndexBefore = context->m_lexicalBlockIndex;
        ByteCodeBlock::ByteCodeLexicalBlockContext blockContext;
        if (m_class.classBodyLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            ASSERT(classIdent);
            context->m_lexicalBlockIndex = m_class.classBodyLexicalBlockIndex();
            InterpretedCodeBlock::BlockInfo* bi = codeBlock->m_codeBlock->blockInfo(m_class.classBodyLexicalBlockIndex());
            blockContext = codeBlock->pushLexicalBlock(context, bi, this);
        }

        if (m_class.superClass() != nullptr) {
            m_class.superClass()->generateExpressionByteCode(codeBlock, context, context->m_classInfo.m_superIndex);
        }

        if (m_class.classBody()->hasConstructor()) {
            m_class.classBody()->constructor()->generateExpressionByteCode(codeBlock, context, dstIndex);
        } else {
            codeBlock->pushCode(CreateClass(ByteCodeLOC(m_loc.index), dstIndex, context->m_classInfo.m_bodyIndex, context->m_classInfo.m_superIndex, context->m_classInfo.m_name, nullptr), context, this);
        }

        m_class.classBody()->generateClassInitializer(codeBlock, context, dstIndex);

        if (m_class.classBodyLexicalBlockIndex() != LEXICAL_BLOCK_INDEX_MAX) {
            ASSERT(classIdent);
            // Initialize class name
            context->m_isLexicallyDeclaredBindingInitialization = true;
            classIdent->generateStoreByteCode(codeBlock, context, dstIndex, false);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);

            codeBlock->finalizeLexicalBlock(context, blockContext);
            context->m_lexicalBlockIndex = lexicalBlockIndexBefore;
        }

        if (context->m_classInfo.m_superIndex != SIZE_MAX) {
            context->giveUpRegister(); // for drop m_superIndex
        }

        context->giveUpRegister(); // for drop m_bodyIndex
        codeBlock->m_shouldClearStack = true;

        context->m_classInfo = classInfoBefore;
    }

private:
    ClassNode m_class;
};
}

#endif
