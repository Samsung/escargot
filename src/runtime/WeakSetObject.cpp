/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "WeakSetObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakSetObject::WeakSetObject(ExecutionState& state)
    : WeakSetObject(state, state.context()->globalObject()->weakSetPrototype())
{
}

WeakSetObject::WeakSetObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

void* WeakSetObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(WeakSetObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(WeakSetObject, m_storage));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(WeakSetObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool WeakSetObject::deleteOperation(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]->key.unwrap() == key) {
            GC_unregister_disappearing_link(reinterpret_cast<void**>(&m_storage[i]->key));
            m_storage.erase(i);
            return true;
        }
    }
    return false;
}

void WeakSetObject::add(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]->key.unwrap() == key) {
            return;
        }
    }

    for (size_t i = 0; i < m_storage.size(); i++) {
        if (!m_storage[i]->key) {
            m_storage[i]->key = key;
            GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&m_storage[i]->key), key);
            return;
        }
    }

    auto newData = new WeakSetObjectDataItem(key);
    m_storage.pushBack(newData);

    GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&newData->key), key);
}

bool WeakSetObject::has(ExecutionState& state, PointerValue* key)
{
    ASSERT(key->isObject() || key->isSymbol());
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i]->key.unwrap() == key) {
            return true;
        }
    }
    return false;
}

} // namespace Escargot
