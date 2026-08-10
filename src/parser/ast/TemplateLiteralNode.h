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
        size_t totalElements = m_expressions.size() + m_quasis->size();

        if (totalElements <= TemplateOperation::kMaxTemplateRegisterCount) {
            size_t rStart = context->getRegisters(totalElements);

            for (size_t i = 0; i < m_quasis->size(); ++i) {
                Value value;
                if ((*m_quasis)[i]->value) {
                    UTF16StringData& sd = (*m_quasis)[i]->value.value();
                    if (sd.size()) {
                        String* str = new UTF16String(std::move((*m_quasis)[i]->value.value()));
                        codeBlock->m_stringLiteralData.push_back(str);
                        value = str;
                    } else {
                        value = String::emptyString();
                    }
                }
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), rStart + (i * 2), value), context, this->m_loc.index);
            }

            size_t idx = 0;
            for (SentinelNode* expression = m_expressions.begin(); expression != m_expressions.end(); expression = expression->next()) {
                expression->astNode()->generateExpressionByteCode(codeBlock, context, rStart + (idx * 2) + 1);
                idx++;
            }

            codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), rStart, totalElements, dstRegister, TemplateOperation::Finalize), context, this->m_loc.index);
            context->giveUpRegisters(totalElements);
        } else {
            size_t rStart = context->getRegisters(TemplateOperation::kMaxTemplateRegisterCount);

            size_t currentChunkCount = 0;
            size_t processedCount = 0;
            SentinelNode* exprIter = m_expressions.begin();

            for (size_t i = 0; i < totalElements; ++i) {
                size_t slotIndex = currentChunkCount;
                if (currentChunkCount == 0 && processedCount > 0) {
                    codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), dstRegister, rStart), context, this->m_loc.index);
                    slotIndex = 1;
                    currentChunkCount = 1;
                }

                if (i % 2 == 0) {
                    size_t quasiIdx = i / 2;
                    Value value;
                    if ((*m_quasis)[quasiIdx]->value) {
                        UTF16StringData& sd = (*m_quasis)[quasiIdx]->value.value();
                        if (sd.size()) {
                            String* str = new UTF16String(std::move((*m_quasis)[quasiIdx]->value.value()));
                            codeBlock->m_stringLiteralData.push_back(str);
                            value = str;
                        } else {
                            value = String::emptyString();
                        }
                    }
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), rStart + slotIndex, value), context, this->m_loc.index);
                } else {
                    exprIter->astNode()->generateExpressionByteCode(codeBlock, context, rStart + slotIndex);
                    exprIter = exprIter->next();
                }

                currentChunkCount++;
                processedCount++;

                if (currentChunkCount == TemplateOperation::kMaxTemplateRegisterCount || processedCount == totalElements) {
                    codeBlock->pushCode(TemplateOperation(ByteCodeLOC(m_loc.index), rStart, currentChunkCount, dstRegister, TemplateOperation::Finalize), context, this->m_loc.index);
                    currentChunkCount = 0;
                }
            }

            context->giveUpRegisters(TemplateOperation::kMaxTemplateRegisterCount);
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
