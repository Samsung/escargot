/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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

#ifndef TemplateLiteralNode_h
#define TemplateLiteralNode_h

#include "Node.h"

namespace Escargot {

struct TemplateElement : public gc {
    Optional<UTF16StringData> value;
    UTF16StringData valueRaw;
    bool tail;
};

typedef Vector<TemplateElement*, GCUtil::gc_malloc_allocator<TemplateElement*>> TemplateElementVector;

class TemplateLiteralNode : public ExpressionNode {
public:
    TemplateLiteralNode(TemplateElementVector* vector, const NodeList& expressions)
        : ExpressionNode()
        , m_quasis(vector)
        , m_expressions(expressions)
    {
    }

    TemplateElementVector* quasis()
    {
        return m_quasis;
    }

    NodeList& expressions()
    {
        return m_expressions;
    }

    virtual ASTNodeType type() override { return ASTNodeType::TemplateLiteral; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        ASSERT(m_expressions.size() + 1 == m_quasis->size());
        Value value;
        if ((*m_quasis)[0]->value) {
            String* str = new UTF16String(std::move((*m_quasis)[0]->value.value()));
            codeBlock->m_stringLiteralData.push_back(str);
            value = str;
        }
        codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), dstRegister, value), context, this);

        size_t index = 0;
        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
            size_t eSrc = expression->astNode()->getRegister(codeBlock, context);
            expression->astNode()->generateExpressionByteCode(codeBlock, context, eSrc);
            codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), dstRegister, eSrc, dstRegister), context, this);
            context->giveUpRegister();

            if ((*m_quasis)[index + 1]->value) {
                String* str = new UTF16String(std::move((*m_quasis)[index + 1]->value.value()));
                if (str->length()) {
                    codeBlock->m_stringLiteralData.push_back(str);
                    size_t reg = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), reg, Value(str)), context, this);
                    codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), dstRegister, reg, dstRegister), context, this);
                    context->giveUpRegister();
                }
            } else {
                size_t reg = context->getRegister();
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), reg, Value()), context, this);
                codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), dstRegister, reg, dstRegister), context, this);
                context->giveUpRegister();
            }
            index++;
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
            expression->astNode()->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
            expression->astNode()->iterateChildren(fn);
        }
    }

private:
    TemplateElementVector* m_quasis;
    NodeList m_expressions;
};
} // namespace Escargot

#endif
