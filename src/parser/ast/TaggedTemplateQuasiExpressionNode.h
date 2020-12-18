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

#ifndef TaggedTemplateQuasiExpressionNode_h
#define TaggedTemplateQuasiExpressionNode_h

#include "Node.h"
#include "ExpressionNode.h"

namespace Escargot {

class TaggedTemplateQuasiExpressionNode : public ExpressionNode {
public:
    TaggedTemplateQuasiExpressionNode(Node* generator, size_t cacheIndex)
        : ExpressionNode()
        , m_generatorNode(generator)
        , m_cacheIndex(cacheIndex)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::TaggedTemplateQuasiExpression; }
    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        TaggedTemplateOperation::TestCacheOperationData testCache;
        testCache.m_cacheIndex = m_cacheIndex;
        testCache.m_jumpPosition = SIZE_MAX;
        testCache.m_registerIndex = dstRegister;
        size_t testCachePos = codeBlock->currentCodeSize();
        codeBlock->pushCode(TaggedTemplateOperation(ByteCodeLOC(m_loc.index), testCache), context, this);

        // create quasi if there is no cache value
        m_generatorNode->generateExpressionByteCode(codeBlock, context, dstRegister);

        codeBlock->peekCode<TaggedTemplateOperation>(testCachePos)->m_testCacheOperationData.m_jumpPosition = codeBlock->currentCodeSize();

        // fill cache
        TaggedTemplateOperation::FillCacheOperationData fillCache;
        fillCache.m_cacheIndex = m_cacheIndex;
        fillCache.m_registerIndex = dstRegister;
        codeBlock->pushCode(TaggedTemplateOperation(ByteCodeLOC(m_loc.index), fillCache), context, this);
    }

    virtual void iterateChildren(const std::function<void(Node* node)>& fn) override
    {
        fn(this);

        m_generatorNode->iterateChildren(fn);
    }

    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        m_generatorNode->iterateChildrenIdentifier(fn);
    }

private:
    Node* m_generatorNode;
    size_t m_cacheIndex;
};
} // namespace Escargot

#endif
