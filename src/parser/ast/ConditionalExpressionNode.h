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

#ifndef ConditionalExpressionNode_h
#define ConditionalExpressionNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ConditionalExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ConditionalExpressionNode(Node* test, Node* consequente, Node* alternate)
        : ExpressionNode()
    {
        m_test = (ExpressionNode*)test;
        m_consequente = (ExpressionNode*)consequente;
        m_alternate = (ExpressionNode*)alternate;
    }
    virtual ~ConditionalExpressionNode()
    {
        delete m_test;
        delete m_consequente;
        delete m_alternate;
    }

    virtual ASTNodeType type() { return ASTNodeType::ConditionalExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister)
    {
        size_t resultRegisterExpected = dstRegister;

        size_t testReg = m_test->getRegister(codeBlock, context);
        m_test->generateExpressionByteCode(codeBlock, context, testReg);
        codeBlock->pushCode(JumpIfFalse(ByteCodeLOC(m_loc.index), testReg), context, this);

        // give testReg
        context->giveUpRegister();

        size_t jumpPosForTestIsFalse = codeBlock->lastCodePosition<JumpIfFalse>();
        m_consequente->generateExpressionByteCode(codeBlock, context, dstRegister);
        codeBlock->pushCode(Jump(ByteCodeLOC(m_loc.index), SIZE_MAX), context, this);
        JumpIfFalse* jumpForTestIsFalse = codeBlock->peekCode<JumpIfFalse>(jumpPosForTestIsFalse);
        size_t jumpPosForEndOfConsequence = codeBlock->lastCodePosition<Jump>();

        jumpForTestIsFalse->m_jumpPosition = codeBlock->currentCodeSize();
        m_alternate->generateExpressionByteCode(codeBlock, context, dstRegister);

        Jump* jumpForEndOfConsequence = codeBlock->peekCode<Jump>(jumpPosForEndOfConsequence);
        jumpForEndOfConsequence->m_jumpPosition = codeBlock->currentCodeSize();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name)>& fn)
    {
        m_test->iterateChildrenIdentifier(fn);
        m_consequente->iterateChildrenIdentifier(fn);
        m_alternate->iterateChildrenIdentifier(fn);
    }

protected:
    ExpressionNode* m_test;
    ExpressionNode* m_consequente;
    ExpressionNode* m_alternate;
};
}

#endif
