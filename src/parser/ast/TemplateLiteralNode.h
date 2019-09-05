/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef TemplateLiteralNode_h
#define TemplateLiteralNode_h

#include "Node.h"

namespace Escargot {

struct TemplateElement : public gc {
    UTF16StringData value;
    UTF16StringData valueRaw;
    bool tail;
};

typedef Vector<TemplateElement*, GCUtil::gc_malloc_allocator<TemplateElement*>> TemplateElementVector;

class TemplateLiteralNode : public ExpressionNode {
public:
    TemplateLiteralNode(TemplateElementVector* vector, ExpressionNodeVector&& expressions)
        : ExpressionNode()
        , m_quasis(vector)
        , m_expressions(std::move(expressions))
    {
    }

    TemplateElementVector* quasis()
    {
        return m_quasis;
    }

    const ExpressionNodeVector& expressions()
    {
        return m_expressions;
    }

    virtual ASTNodeType type() { return ASTNodeType::TemplateLiteral; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        ASSERT(m_expressions.size() + 1 == m_quasis->size());
        String* str = new UTF16String(std::move((*m_quasis)[0]->value));
        codeBlock->m_literalData.push_back(str);
        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, Value(str)), context, this);

        for (size_t i = 0; i < m_expressions.size(); i++) {
            size_t eSrc = m_expressions[i]->getRegister(codeBlock, context);
            m_expressions[i]->generateExpressionByteCode(codeBlock, context, eSrc);
            codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), dstRegister, eSrc, dstRegister), context, this);
            context->giveUpRegister();

            String* str = new UTF16String(std::move((*m_quasis)[i + 1]->value));
            if (str->length()) {
                codeBlock->m_literalData.push_back(str);
                size_t reg = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), reg, Value(str)), context, this);
                codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), dstRegister, reg, dstRegister), context, this);
                context->giveUpRegister();
            }
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        for (size_t i = 0; i < m_expressions.size(); i++) {
            m_expressions[i]->iterateChildrenIdentifier(fn);
        }
    }

private:
    TemplateElementVector* m_quasis;
    ExpressionNodeVector m_expressions;
};
}

#endif
