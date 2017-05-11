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

#ifndef UnaryExpressionMinusNode_h
#define UnaryExpressionMinusNode_h

#include "ExpressionNode.h"

namespace Escargot {

class UnaryExpressionMinusNode : public ExpressionNode {
public:
    friend class ScriptParser;
    UnaryExpressionMinusNode(Node* argument)
        : ExpressionNode()
    {
        m_argument = argument;
    }
    virtual ~UnaryExpressionMinusNode()
    {
        delete m_argument;
    }


    virtual ASTNodeType type() { return ASTNodeType::UnaryExpressionMinus; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t srcIndex = m_argument->getRegister(codeBlock, context);
        m_argument->generateExpressionByteCode(codeBlock, context, srcIndex);
        context->giveUpRegister();
        codeBlock->pushCode(UnaryMinus(ByteCodeLOC(m_loc.index), srcIndex, dstRegister), context, this);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name)>& fn)
    {
        m_argument->iterateChildrenIdentifier(fn);
    }

protected:
    Node* m_argument;
};
}

#endif
