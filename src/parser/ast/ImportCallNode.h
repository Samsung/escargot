/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef ImportCallNode_h
#define ImportCallNode_h

#include "ExpressionNode.h"

namespace Escargot {

class ImportCallNode : public ExpressionNode {
public:
    ImportCallNode(Node* specifier, Node* options)
        : ExpressionNode()
        , m_specifier(specifier)
        , m_options(options)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::ImportCall; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        ByteCodeRegisterIndex specifierReg = context->getRegister();
        ByteCodeRegisterIndex optionsReg = 0;

        if (m_options) {
            optionsReg = context->getRegister();
        }

        m_specifier->generateExpressionByteCode(codeBlock, context, specifierReg);

        if (m_options) {
            m_options->generateExpressionByteCode(codeBlock, context, optionsReg);
        }

        codeBlock->pushCode(CallComplexCase(ByteCodeLOC(m_loc.index), CallComplexCase::Import, false, false, false,
                                            SIZE_MAX, SIZE_MAX, specifierReg, dstRegister, m_options ? 2 : 1),
                            context, this->m_loc.index);


        if (m_options) {
            context->giveUpRegister();
        }

        context->giveUpRegister();
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_specifier->iterateChildrenIdentifier(fn);
        if (m_options) {
            m_options->iterateChildrenIdentifier(fn);
        }
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);
        m_specifier->iterateChildren(fn);
        if (m_options) {
            m_options->iterateChildren(fn);
        }
    }

private:
    Node* m_specifier;
    Node* m_options;
};
} // namespace Escargot

#endif
