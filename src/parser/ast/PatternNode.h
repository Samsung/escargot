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

#ifndef PatternNodeNode_h
#define PatternNodeNode_h

#include "Node.h"

namespace Escargot {

class PatternNode : public Node {
public:
    PatternNode(RefPtr<Node> init = nullptr)
        : Node()
        , m_init(init)
        , m_initIdx(SIZE_MAX)
    {
    }

    PatternNode(size_t initIdx)
        : Node()
        , m_init(nullptr)
        , m_initIdx(initIdx)
    {
    }

    void setInitializer(RefPtr<Node> init)
    {
        m_init = init;
    }

    virtual PatternNode* asPattern(RefPtr<Node> init)
    {
        m_init = init;
        return this;
    }

    virtual PatternNode* asPattern(size_t initIdx)
    {
        m_initIdx = initIdx;
        return this;
    }

    virtual PatternNode* asPattern()
    {
        return this;
    }

    virtual bool isPattern()
    {
        return true;
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        context->getRegister();
    }

    virtual void generateResultNotRequiredExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        generateExpressionByteCode(codeBlock, context, REGISTER_LIMIT);
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
    {
        m_initIdx = srcRegister;
        generateResultNotRequiredExpressionByteCode(codeBlock, context);
    }


protected:
    RefPtr<Node> m_init;
    size_t m_initIdx;
};
}

#endif
