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

#include "Escargot.h"
#include "FinalizationRegistryObject.h"
#include "Context.h"

namespace Escargot {

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, EncodedValue cleanupCallback, Object* realm)
    : FinalizationRegistryObject(state, state.context()->globalObject()->finalizationRegistryPrototype(), cleanupCallback, realm)
{
}

FinalizationRegistryObject::FinalizationRegistryObject(ExecutionState& state, Object* proto, EncodedValue cleanupCallback, Object* realm)
    : Object(state, proto)
    , m_cleanupCallback(cleanupCallback)
    , m_realm(realm)
{
}

void* FinalizationRegistryObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(FinalizationRegistryObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_cleanupCallback));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_realm));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FinalizationRegistryObject, m_cells));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(FinalizationRegistryObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void FinalizationRegistryObject::setCell(ExecutionState& state, const Value& weakRefTarget, const Value& heldValue, const Value& unregisterToken)
{
    auto newCell = new FinalizationRegistryObjectItem();
    newCell->weakRefTarget = weakRefTarget;
    newCell->heldValue = heldValue;
    newCell->unregisterToken = unregisterToken;
    m_cells.pushBack(newCell);
}

bool FinalizationRegistryObject::deleteCell(ExecutionState& state, Value unregisterToken)
{
    bool removed = false;
    for (size_t i = 0; i < m_cells.size(); i++) {
        if (unregisterToken.equalsToByTheSameValueAlgorithm(state, m_cells[i]->unregisterToken)) {
            m_cells.erase(i);
            i--;
            removed = true;
        }
    }
    return removed;
}

} // namespace Escargot
