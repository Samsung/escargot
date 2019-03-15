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

#ifndef ClassExpressionNode_h
#define ClassExpressionNode_h

#include "ClassNode.h"
#include "ClassBodyNode.h"
#include "FunctionExpressionNode.h"
#include "ExpressionNode.h"

namespace Escargot {

class ClassExpressionNode : public ExpressionNode {
public:
    friend class ScriptParser;
    ClassExpressionNode(RefPtr<IdentifierNode> id, RefPtr<Node> superClass, RefPtr<ClassBodyNode> classBody, Context* ctx)
        : ExpressionNode()
        , m_class(id, superClass, classBody)
        , m_ctx(ctx)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ClassExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstIndex)
    {
        if (m_class.classBody()->hasConstructor()) {
            m_class.classBody()->constructor()->generateExpressionByteCode(codeBlock, context, dstIndex);
        } else {
            codeBlock->pushCode(CreateImplicitConstructor(ByteCodeLOC(m_loc.index), dstIndex), context, this);
        }

        m_class.classBody()->generateClassInitializer(codeBlock, context, dstIndex, m_ctx);
    }

private:
    ClassNode m_class;
    Context* m_ctx;
};
}

#endif
