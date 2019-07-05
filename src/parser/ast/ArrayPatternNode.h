/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef ArrayPatternNode_h
#define ArrayPatternNode_h

#include "AssignmentExpressionSimpleNode.h"
#include "IdentifierNode.h"
#include "ExpressionNode.h"
#include "Node.h"
#include "PatternNode.h"
#include "RegisterReferenceNode.h"

namespace Escargot {

class ArrayPatternNode : public PatternNode {
public:
    friend class ScriptParser;
    ArrayPatternNode(ExpressionNodeVector&& elements, RefPtr<Node> init = nullptr, bool hasRestElement = false)
        : PatternNode(init)
        , m_elements(elements)
        , m_default(nullptr)
        , m_hasRestElement(hasRestElement)
    {
    }

    ArrayPatternNode(ExpressionNodeVector&& elements, size_t initIdx)
        : PatternNode(initIdx)
        , m_elements(elements)
        , m_default(nullptr)
        , m_hasRestElement(false)
    {
    }

    virtual PatternNode* asPattern(RefPtr<Node> init)
    {
        m_default = m_init;
        m_init = init;
        return this;
    }

    virtual ASTNodeType type() { return ASTNodeType::ArrayPattern; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool generateRightSide = m_initIdx == SIZE_MAX;
        bool noResult = dstRegister == REGISTER_LIMIT;

        if (generateRightSide) {
            m_initIdx = noResult ? context->getRegister() : dstRegister;
            m_init->generateExpressionByteCode(codeBlock, context, m_initIdx);
        }

        size_t iteratorIdx = context->getRegister();

        if (m_default != nullptr) {
            size_t undefinedIndex = context->getRegister();
            codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), undefinedIndex, PropertyName(codeBlock->m_codeBlock->context()->staticStrings().undefined)), context, this);

            size_t cmpIndex = context->getRegister();
            codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), m_initIdx, undefinedIndex, cmpIndex), context, this);

            context->giveUpRegister(); // for drop undefinedIndex

            codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
            size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();

            context->giveUpRegister(); // for drop cmpIndex

            m_default->generateExpressionByteCode(codeBlock, context, m_initIdx);

            codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
        }

        codeBlock->pushCode(GetIterator(ByteCodeLOC(m_loc.index), m_initIdx, iteratorIdx), context, this);

        size_t bindingCount = m_hasRestElement ? m_elements.size() - 1 : m_elements.size();

        for (size_t i = 0; i < bindingCount; i++) {
            RefPtr<Node> element = m_elements[i];

            size_t iteratorValueIdx = context->getRegister();
            codeBlock->pushCode(IteratorStep(ByteCodeLOC(m_loc.index), iteratorValueIdx, iteratorIdx), context, this);

            if (element != nullptr) {
                if (element->isPattern()) {
                    Node* pattern = element.get()->asPattern(iteratorValueIdx);
                    pattern->generateResultNotRequiredExpressionByteCode(codeBlock, context);
                    context->giveUpRegister();
                } else if (element->isAssignmentExpressionSimple()) {
                    AssignmentExpressionSimpleNode* assignNode = element->asAssignmentExpressionSimple();
                    size_t undefinedIndex = context->getRegister();
                    codeBlock->pushCode(GetGlobalObject(ByteCodeLOC(m_loc.index), undefinedIndex, PropertyName(codeBlock->m_codeBlock->context()->staticStrings().undefined)), context, this);

                    size_t cmpIndex = context->getRegister();
                    codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), iteratorValueIdx, undefinedIndex, cmpIndex), context, this);

                    context->giveUpRegister(); // for drop undefinedIndex

                    codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
                    size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();

                    context->giveUpRegister(); // for drop cmpIndex

                    assignNode->generateExpressionByteCode(codeBlock, context, iteratorValueIdx);
                    codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index)), context, this);
                    size_t jPos = codeBlock->lastCodePosition<Jump>();
                    codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();

                    RefPtr<RegisterReferenceNode> registerRef = adoptRef(new RegisterReferenceNode(iteratorValueIdx));
                    RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(assignNode->left(), registerRef.get()));
                    assign->m_loc = m_loc;
                    assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
                    assign->giveupChildren();

                    Jump* j = codeBlock->peekCode<Jump>(jPos);
                    j->m_jumpPosition = codeBlock->currentCodeSize();

                    if (assignNode->left()->isIdentifier() && context->m_isConstDeclaration == true) {
                        codeBlock->pushCode(SetConstBinding(ByteCodeLOC(m_loc.index), assignNode->left()->asIdentifier()->name()), context, this);
                    }
                } else {
                    RefPtr<RegisterReferenceNode> registerRef = adoptRef(new RegisterReferenceNode(iteratorValueIdx));
                    RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(element.get(), registerRef.get()));
                    assign->m_loc = m_loc;
                    assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
                    assign->giveupChildren();

                    if (element->isIdentifier() && context->m_isConstDeclaration == true) {
                        codeBlock->pushCode(SetConstBinding(ByteCodeLOC(m_loc.index), element->asIdentifier()->name()), context, this);
                    }
                }
            }
        }

        if (m_hasRestElement) {
            RefPtr<Node> element = m_elements[m_elements.size() - 1];
            size_t restIndex = context->getRegister();
            codeBlock->pushCode(BindingRestElement(ByteCodeLOC(m_loc.index), iteratorIdx, restIndex), context, this);
            RefPtr<RegisterReferenceNode> registerRef = adoptRef(new RegisterReferenceNode(restIndex));
            RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(element.get(), registerRef.get()));
            assign->m_loc = m_loc;
            assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            assign->giveupChildren();
        }

        context->giveUpRegister(); // for iteratorIdx
        if (generateRightSide && noResult) {
            context->giveUpRegister(); // for drop m_initIdx
        }
    }

private:
    ExpressionNodeVector m_elements;
    RefPtr<Node> m_default;
    bool m_hasRestElement : 1;
};
}

#endif
