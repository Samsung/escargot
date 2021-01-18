/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotFinalizationRegistryObject__
#define __EscargotFinalizationRegistryObject__

#include "runtime/Object.h"

namespace Escargot {

class FinalizationRegistryObject : public Object {
public:
    struct FinalizationRegistryObjectItem : public gc {
        EncodedValue weakRefTarget;
        EncodedValue heldValue;
        EncodedValue unregisterToken;
    };

    typedef Vector<FinalizationRegistryObjectItem*, GCUtil::gc_malloc_allocator<FinalizationRegistryObjectItem*>> FinalizationRegistryObjectCells;

    explicit FinalizationRegistryObject(ExecutionState& state, EncodedValue cleanupCallback, Object* realm);
    explicit FinalizationRegistryObject(ExecutionState& state, Object* proto, EncodedValue cleanupCallback, Object* realm);

    virtual bool isFinalizationRegistryObject() const
    {
        return true;
    }

    void setCell(ExecutionState& state, const Value& weakRefTarget, const Value& heldValue, const Value& unregisterToken);
    bool deleteCell(ExecutionState& state, Value unregisterToken);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    Optional<EncodedValue> m_cleanupCallback;
    Optional<Object*> m_realm;
    FinalizationRegistryObjectCells m_cells;
};
} // namespace Escargot

#endif
