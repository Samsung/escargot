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

class FinalizationRegistryObject : public DerivedObject {
    friend class CleanupSomeJob;

public:
    struct FinalizationRegistryObjectItem : public gc {
#if !defined(NDEBUG)
        friend int getValidValueInFinalizationRegistryObjectItem(void* ptr, GC_mark_custom_result* arr);
#endif
        PointerValue* weakRefTarget; // weak reference (should have a valid address)
        EncodedValue heldValue;
        FinalizationRegistryObject* source;
        Optional<PointerValue*> unregisterToken; // weak reference (could be null)

        void reset()
        {
            weakRefTarget = nullptr;
            heldValue = Value();
            source = nullptr;
            unregisterToken.reset();
        }

        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;
    };

    typedef Vector<FinalizationRegistryObjectItem*, GCUtil::gc_malloc_allocator<FinalizationRegistryObjectItem*>> FinalizationRegistryObjectCells;

    explicit FinalizationRegistryObject(ExecutionState& state, Object* cleanupCallback, Context* realm);
    explicit FinalizationRegistryObject(ExecutionState& state, Object* proto, Object* cleanupCallback, Context* realm);

    virtual bool isFinalizationRegistryObject() const
    {
        return true;
    }

    void setCell(PointerValue* weakRefTarget, const Value& heldValue, Optional<PointerValue*> unregisterToken);
    bool deleteCell(PointerValue* unregisterToken);
    bool deleteCellOnly(FinalizationRegistryObjectItem* item);
    void cleanupSome(ExecutionState& state, Optional<Object*> callback);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    static void finalizer(PointerValue* self, void* data);
    static void finalizerUnregisterToken(PointerValue* self, void* data);

    void tryToShrinkCells();

    Object* m_cleanupCallback;
    Context* m_realm;
    FinalizationRegistryObjectCells m_cells;
    size_t m_deletedCellCount;
};
} // namespace Escargot

#endif
