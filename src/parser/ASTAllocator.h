/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotASTAllocator__
#define __EscargotASTAllocator__

namespace Escargot {

class Node;

class ASTAllocator {
public:
    typedef intptr_t ASTAllocAlignment;

    ASTAllocator();
    ~ASTAllocator();

    void reset();
    void* allocate(size_t size);

    bool isInitialized()
    {
        ASSERT(m_astPools.size() == 0);
        return (m_astPoolMemory == currentPool());
    }

private:
    inline size_t astPoolSize() const
    {
        const size_t astPoolSizeMap[] = {
            1024 * 4,
            1024 * 16,
            1024 * 128
        };
        if (m_astPools.size() >= (sizeof(astPoolSizeMap) / sizeof(size_t))) {
            return astPoolSizeMap[(sizeof(astPoolSizeMap) / sizeof(size_t)) - 1];
        }
        return astPoolSizeMap[m_astPools.size()];
    }

    size_t alignSize(size_t size)
    {
        return (size + sizeof(ASTAllocAlignment) - 1) & ~(sizeof(ASTAllocAlignment) - 1);
    }
    void allocatePool();
    void* currentPool()
    {
        ASSERT(m_astPoolMemory != nullptr && m_astPoolEnd != nullptr);
        ASSERT(static_cast<size_t>(m_astPoolEnd - m_astPoolMemory) >= 0);

        return m_astPoolEnd - astPoolSize();
    }

    char* m_astPoolMemory;
    char* m_astPoolEnd;

    std::vector<void*> m_astPools;
};
} // namespace Escargot
#endif
