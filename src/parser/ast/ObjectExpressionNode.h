/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    ObjectExpressionNode(PropertiesNodeVector&& properties)
        : ExpressionNode()
    {
        m_properties = properties;
    }

    virtual ~ObjectExpressionNode()
    {
        for (unsigned i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i];
            delete p;
        }
    }

    virtual ASTNodeType type() { return ASTNodeType::ObjectExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), context->getRegister()), context, this);
        size_t objIndex = context->getLastRegisterIndex();
        for (unsigned i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i];
            AtomicString propertyAtomicName;
            if (p->key()->isIdentifier()) {
                propertyAtomicName = p->key()->asIdentifier()->name();
                // we can use LoadLiteral here
                // because, p->key()->asIdentifier()->name().string()
                // is protected by AtomicString (IdentifierNode always has AtomicString)
                codeBlock->pushCode(LoadLiteral(ByteCodeLOC(m_loc.index), context->getRegister(), Value(p->key()->asIdentifier()->name().string())), context, this);
            } else {
                p->key()->generateExpressionByteCode(codeBlock, context);
            }
            size_t propertyIndex = context->getLastRegisterIndex();

            p->value()->generateExpressionByteCode(codeBlock, context);

            size_t valueIndex = context->getLastRegisterIndex();

            if (p->kind() == PropertyNode::Kind::Init) {
                codeBlock->pushCode(ObjectDefineOwnPropertyOperation(ByteCodeLOC(m_loc.index), objIndex, propertyIndex, valueIndex), context, this);

                // for drop property index
                context->giveUpRegister();
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
        }
        ASSERT(objIndex == context->getLastRegisterIndex());
    }

protected:
    PropertiesNodeVector m_properties;
};
}

#endif
