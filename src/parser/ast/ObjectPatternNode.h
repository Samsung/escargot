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

#ifndef ObjectPatternNode_h
#define ObjectPatternNode_h

#include "AssignmentExpressionSimpleNode.h"
#include "IdentifierNode.h"
#include "ExpressionNode.h"
#include "Node.h"
#include "PatternNode.h"
#include "PropertyNode.h"
#include "RegisterReferenceNode.h"

namespace Escargot {

class ObjectPatternNode : public PatternNode {
public:
    friend class ScriptParser;
    ObjectPatternNode(PropertiesNodeVector&& properties, RefPtr<Node> init = nullptr)
        : PatternNode(init)
        , m_properties(properties)
    {
    }

    ObjectPatternNode(PropertiesNodeVector&& properties, size_t initIdx)
        : PatternNode(initIdx)
        , m_properties(properties)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ObjectPattern; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        bool generateRightSide = m_initIdx == SIZE_MAX;
        bool noResult = dstRegister == REGISTER_LIMIT;

        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;

        if (generateRightSide) {
            m_initIdx = noResult ? context->getRegister() : dstRegister;
            m_init->generateExpressionByteCode(codeBlock, context, m_initIdx);
        }

        size_t objIndex = m_initIdx;
        for (unsigned i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i].get();
            AtomicString propertyAtomicName;
            bool hasKey = false;
            size_t propertyIndex = SIZE_MAX;
            if (p->key()->isIdentifier() && !p->computed()) {
                if (p->kind() == PropertyNode::Kind::Init) {
                    propertyAtomicName = p->key()->asIdentifier()->name();
                    hasKey = true;
                } else {
                    propertyIndex = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), propertyIndex, Value(p->key()->asIdentifier()->name().string())), context, this);
                }
            } else {
                propertyIndex = p->key()->getRegister(codeBlock, context);
                p->key()->generateExpressionByteCode(codeBlock, context, propertyIndex);
            }

            Node* value = p->value();
            size_t valueIndex = context->getRegister();

            if (value != nullptr && !value->isPattern()) {
                size_t undefinedIndex = context->getRegister();
                codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), undefinedIndex, PropertyName(codeBlock->m_codeBlock->context()->staticStrings().undefined)), context, this);

                size_t cmpIndex = context->getRegister();
                codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), objIndex, undefinedIndex, cmpIndex), context, this);

                context->giveUpRegister(); // for drop undefinedIndex

                codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
                size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();

                context->giveUpRegister(); // for drop cmpIndex

                value->generateExpressionByteCode(codeBlock, context, objIndex);

                codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();
            } else {
                if (hasKey) {
                    codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objIndex, valueIndex, propertyAtomicName), context, this);
                } else {
                    codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);
                }
            }

            if (hasKey) {
                codeBlock->pushCode(GetObjectPreComputedCase(ByteCodeLOC(m_loc.index), objIndex, valueIndex, propertyAtomicName), context, this);
            } else {
                codeBlock->pushCode(GetObject(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);
            }

            Node* key = nullptr;
            size_t jPos = 0;
            if (value != nullptr) {
                if (value->isPattern()) {
                    Node* pattern = value->asPattern(valueIndex);
                    pattern->generateResultNotRequiredExpressionByteCode(codeBlock, context);
                    context->giveUpRegister();
                    continue;
                }

                size_t undefinedIndex = context->getRegister();
                codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), undefinedIndex, PropertyName(codeBlock->m_codeBlock->context()->staticStrings().undefined)), context, this);

                size_t cmpIndex = context->getRegister();
                codeBlock->pushCode(BinaryStrictEqual(ByteCodeLOC(m_loc.index), valueIndex, undefinedIndex, cmpIndex), context, this);

                context->giveUpRegister(); // for drop undefinedIndex

                codeBlock->pushCode<JumpIfFalse>(JumpIfFalse(ByteCodeLOC(m_loc.index), cmpIndex), context, this);
                size_t pos = codeBlock->lastCodePosition<JumpIfFalse>();

                context->giveUpRegister(); // for drop cmpIndex
                value->generateExpressionByteCode(codeBlock, context, valueIndex);
                codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index)), context, this);
                jPos = codeBlock->lastCodePosition<Jump>();
                codeBlock->peekCode<JumpIfFalse>(pos)->m_jumpPosition = codeBlock->currentCodeSize();

                switch (value->type()) {
                case AssignmentExpressionSimple: {
                    key = value->asAssignmentExpressionSimple()->left();
                    break;
                }
                case DefaultArgument: {
                    key = value->asDefaultArgument()->left();
                    break;
                }
                case Identifier: {
                    key = value;
                    break;
                }
                default: {
                    break;
                }
                }
            }

            if (key == nullptr) {
                ByteCodeGenerateError err;
                err.m_index = m_loc.index;
                throw err;
            }

            RefPtr<RegisterReferenceNode> registerRef = adoptRef(new RegisterReferenceNode(valueIndex));
            RefPtr<AssignmentExpressionSimpleNode> assign = adoptRef(new AssignmentExpressionSimpleNode(key, registerRef.get()));
            assign->m_loc = m_loc;
            context->m_isLexicallyDeclaredBindingInitialization = isLexicallyDeclaredBindingInitialization;
            assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            assign->giveupChildren();

            Jump* j = codeBlock->peekCode<Jump>(jPos);
            j->m_jumpPosition = codeBlock->currentCodeSize();

            if (!hasKey) {
                context->giveUpRegister(); // for drop property index
            }
        }

        if (generateRightSide && noResult) {
            context->giveUpRegister(); // for drop m_initIdx
        }
    }

private:
    PropertiesNodeVector m_properties;
};
}

#endif
