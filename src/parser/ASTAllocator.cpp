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

#include "Escargot.h"
#include "ASTAllocator.h"
#include "parser/ast/Node.h"

namespace Escargot {

ASTAllocator::ASTAllocator()
{
    m_astPoolMemory = static_cast<char*>(malloc(initialASTPoolSize));
    m_astPoolEnd = m_astPoolMemory + initialASTPoolSize;
}

ASTAllocator::~ASTAllocator()
{
    ASSERT(m_astPools.size() == 0);
    ASSERT(m_astDestructibleNodes.size() == 0);

    free(currentPool());
}

void ASTAllocator::reset()
{
    // check if GC is disabled (after reset() GC is enabled again)
    ASSERT(GC_is_disabled());
    ASSERT(m_astPoolMemory != nullptr && m_astPoolEnd != nullptr);
    ASSERT(static_cast<size_t>(m_astPoolEnd - m_astPoolMemory) >= 0);

    for (size_t i = 0; i < m_astDestructibleNodes.size(); i++) {
        m_astDestructibleNodes[i]->~DestructibleNode();
    }
    m_astDestructibleNodes.clear();

    for (size_t i = 0; i < m_astPools.size(); i++) {
        free(m_astPools[i]);
    }
    m_astPools.clear();

    m_astPoolMemory = static_cast<char*>(currentPool());
}

void* ASTAllocator::allocate(size_t size)
{
    ASSERT(size > 0);
    ASSERT(size <= initialASTPoolSize);
    size_t alignedSize = alignSize(size);
    ASSERT(alignedSize <= initialASTPoolSize);
    if (UNLIKELY(static_cast<size_t>(m_astPoolEnd - m_astPoolMemory) < alignedSize)) {
        allocatePool();
    }
    void* block = m_astPoolMemory;
    m_astPoolMemory += alignedSize;
    return block;
}

void* ASTAllocator::allocateDestructible(size_t size)
{
    void* ptr = allocate(size);
    DestructibleNode* destructible = static_cast<DestructibleNode*>(ptr);
    m_astDestructibleNodes.push_back(destructible);

    return ptr;
}

void ASTAllocator::allocatePool()
{
    ASSERT(m_astPoolMemory != nullptr && m_astPoolEnd != nullptr);
    ASSERT(static_cast<size_t>(m_astPoolEnd - m_astPoolMemory) >= 0);

    m_astPools.push_back(currentPool());

    char* pool = static_cast<char*>(malloc(initialASTPoolSize));
    m_astPoolMemory = pool;
    m_astPoolEnd = pool + initialASTPoolSize;
}
}
