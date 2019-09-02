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

#include "Node.h"
#include "PropertyNode.h"

namespace Escargot {

class ObjectPatternNode : public Node {
public:
    friend class ScriptParser;
    ObjectPatternNode(PropertiesNodeVector&& properties)
        : m_properties(properties)
    {
    }

    virtual ASTNodeType type() { return ASTNodeType::ObjectPattern; }
    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf)
    {
        for (size_t i = 0; i < m_properties.size(); i++) {
            m_properties[i]->generateStoreByteCode(codeBlock, context, srcRegister, needToReferenceSelf);
        }
    }

    // FIXME implement iterateChildrenIdentifier in PropertyNode itself
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
