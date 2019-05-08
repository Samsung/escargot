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

#ifndef ObjectExpressionNode_h
#define ObjectExpressionNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "PropertyNode.h"

namespace Escargot {

class ObjectExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    explicit ObjectExpressionNode(PropertiesNodeVector&& properties)
        : ExpressionNode()
        , m_properties(properties)
    {
    }

    virtual ~ObjectExpressionNode()
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ObjectExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), dstRegister), context, this);
        size_t objIndex = dstRegister;
        for (unsigned i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i].get();
            AtomicString propertyAtomicName;
            bool hasKey = false;
            size_t propertyIndex = SIZE_MAX;
            if (p->key()->isIdentifier() && !p->computed()) {
                if (p->kind() == PropertyNode::Kind::Init) {
                    // skip
                    propertyAtomicName = p->key()->asIdentifier()->name();
                    hasKey = true;
                } else {
                    // we can use LoadLiteral here
                    // because, p->key()->asIdentifier()->name().string()
                    // is protected by AtomicString (IdentifierNode always has AtomicString)
                    propertyIndex = context->getRegister();
                    codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), propertyIndex, Value(p->key()->asIdentifier()->name().string())), context, this);
                }
            } else {
                propertyIndex = p->key()->getRegister(codeBlock, context);
                p->key()->generateExpressionByteCode(codeBlock, context, propertyIndex);
            }

            size_t valueIndex = p->value()->getRegister(codeBlock, context);
            p->value()->generateExpressionByteCode(codeBlock, context, valueIndex);

            if (p->kind() == PropertyNode::Kind::Init) {
                if (hasKey) {
                    codeBlock->pushCode(ObjectDefineOwnPropertyWithNameOperation(ByteCodeLOC(m_loc.index), objIndex, propertyAtomicName, valueIndex), context, this);
                } else {
                    codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);
                    // for drop property index
                    context->giveUpRegister();
                }
            } else if (p->kind() == PropertyNode::Kind::Get) {
                codeBlock->pushCode(ObjectDefineGetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);
                // for drop property index
                context->giveUpRegister();
            } else {
                ASSERT(p->kind() == PropertyNode::Kind::Set);
                codeBlock->pushCode(ObjectDefineSetter(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);
                // for drop property index
                context->giveUpRegister();
            }

            // for drop value index
            context->giveUpRegister();

            codeBlock->m_shouldClearStack = true;
        }
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn)
    {
        for (size_t i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i].get();
            if (!(p->key()->isIdentifier() && !p->computed())) {
                p->key()->iterateChildrenIdentifier(fn);
            }
        }
    }

private:
    PropertiesNodeVector m_properties;
};
}

#endif
