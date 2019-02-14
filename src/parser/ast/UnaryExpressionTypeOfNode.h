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

#ifndef UnaryExpressionTypeOfNode_h
#define UnaryExpressionTypeOfNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionTypeOfNode : public ExpressionNode {
public:
    friend class ScriptParser;
    explicit UnaryExpressionTypeOfNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = argument;
    }
    virtual ~UnaryExpressionTypeOfNode()
    {
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        if (m_argument->isIdentifier()) {
            AtomicString name = m_argument->asIdentifier()->name();
            bool nameCase = false;
            if (!context->m_codeBlock->canUseIndexedVariableStorage()) {
                nameCase = true;
            } else {
                InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(name);
                if (!info.m_isResultSaved) {
                    nameCase = true;
                }
            }
            if (nameCase) {
                if (name.string()->equals("arguments") && !context->isGlobalScope()) {
                    size_t srcIndex = m_argument->getRegister(codeBlock, context);
                    m_argument->generateExpressionByteCode(codeBlock, context, srcIndex);
                    context->giveUpRegister();
                    codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), srcIndex, dstRegister, AtomicString()), context, this);
                } else {
                    codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), SIZE_MAX, dstRegister, name), context, this);
                }
                return;
            }
        }

        size_t srcIndex = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, srcIndex);
        context->giveUpRegister();
        codeBlock->pushCode(UnaryTypeof(ByteCodeLOC(m_loc.index), srcIndex, dstRegister, AtomicString()), context, this);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        m_argument->iterateChildrenIdentifier(fn);
    }

    virtual ASTNodeType type() { return ASTNodeType::UnaryExpressionTypeOf; }
private:
    RefPtr<Node> m_argument;
};
}

#endif
