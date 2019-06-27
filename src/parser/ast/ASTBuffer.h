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

#ifndef __EscargotASTBuffer__
#define __EscargotASTBuffer__

namespace Escargot {


typedef intptr_t ASTAllocAlignment;

class Node;
class DestructibleNode;

class ASTBuffer {
public:
    ASTBuffer();
    ~ASTBuffer();

    void reset();
    void* allocate(size_t size);
    void* allocateDestructibleNode(size_t size);
    void* allocateGCNode(size_t size);

    bool isInitialized()
    {
        ASSERT(m_astPools.size() == 0);
        ASSERT(m_astGCNodes.size() == 0);
        ASSERT(m_astDestructibleNodes.size() == 0);
        return (m_astPoolMemory == currentPool());
    }

private:
    static const size_t initialASTPoolSize = 4096;

    size_t alignSize(size_t size)
    {
        return (size + sizeof(ASTAllocAlignment) - 1) & ~(sizeof(ASTAllocAlignment) - 1);
    }
    void allocatePool();
    void* currentPool()
    {
        ASSERT(m_astPoolMemory != nullptr && m_astPoolEnd != nullptr);
        ASSERT(static_cast<size_t>(m_astPoolEnd - m_astPoolMemory) >= 0);

        return m_astPoolEnd - initialASTPoolSize;
    }

    char* m_astPoolMemory;
    char* m_astPoolEnd;

    std::vector<void*> m_astPools;
    std::vector<DestructibleNode*> m_astDestructibleNodes;
    std::vector<Node*> m_astGCNodes;
};
}
#endif
