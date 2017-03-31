/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        codeBlock->pushCode(CreateObject(ByteCodeLOC(m_loc.index), dstRegister), context, this);
        size_t objIndex = dstRegister;
        for (unsigned i = 0; i < m_properties.size(); i++) {
            PropertyNode* p = m_properties[i];
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

protected:
    PropertiesNodeVector m_properties;
};
}

#endif
