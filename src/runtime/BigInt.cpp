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

#include "Escargot.h"
#include "BigInt.h"
#include "VMInstance.h"

namespace Escargot {

void* BigInt::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(BigInt)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(BigInt, m_vmInstance));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(BigInt));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

BigInt::BigInt(VMInstance* vmInstance)
    : m_tag(POINTER_VALUE_BIGINT_TAG_IN_DATA)
    , m_vmInstance(vmInstance)
{
    bf_init(m_vmInstance->bfContext(), &m_bf);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        BigInt* self = (BigInt*)obj;
        bf_delete(&self->m_bf);
    },
                                   nullptr, nullptr, nullptr);
}

BigInt::BigInt(VMInstance* vmInstance, int64_t num)
    : BigInt(vmInstance)
{
    bf_set_si(&m_bf, num);
}
}
