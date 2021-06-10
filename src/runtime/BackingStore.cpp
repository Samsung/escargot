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
 *  Lesser General Public License for more detaials.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "BackingStore.h"

namespace Escargot {

void* BackingStore::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        // only mark m_deleterData
        GC_word obj_bitmap[GC_BITMAP_SIZE(BackingStore)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BackingStore, m_deleterData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(BackingStore));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

BackingStore::BackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, void* callbackData, bool isShared)
    : m_isShared(isShared)
    , m_data(data)
    , m_byteLength(byteLength)
    , m_deleter(callback)
    , m_deleterData(callbackData)
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        BackingStore* self = (BackingStore*)obj;
        self->m_deleter(self->m_data, self->m_byteLength, self->m_deleterData);
    },
                                   nullptr, nullptr, nullptr);
}

BackingStore::BackingStore(size_t byteLength)
    : BackingStore(calloc(byteLength, 1), byteLength,
                   [](void* data, size_t length, void* deleterData) {
                       free(data);
                   },
                   nullptr)
{
}

void BackingStore::deallocate()
{
    // Shared Data Block should not be deleted explicitly
    ASSERT(!m_isShared);
    m_deleter(m_data, m_byteLength, m_deleterData);
    GC_REGISTER_FINALIZER_NO_ORDER(this, nullptr, nullptr, nullptr, nullptr);
}

} // namespace Escargot
