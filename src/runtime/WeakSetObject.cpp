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
    addFinalizer([](Object* self, void* data) {
        auto ws = self->asWeakSetObject();
        for (size_t i = 0; i < ws->m_storage.size(); i++) {
            ws->m_storage[i]->removeFinalizer(WeakSetObject::finalizer, ws);
        }
        ws->m_storage.clear();
    },
                 nullptr);
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

bool WeakSetObject::deleteOperation(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i] == key) {
            key->removeFinalizer(finalizer, this);
            m_storage.erase(i);
            return true;
        }
    }
    return false;
}

void WeakSetObject::add(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i] == key) {
            return;
        }
    }

    key->addFinalizer(WeakSetObject::finalizer, this);
    m_storage.pushBack(key);
}

bool WeakSetObject::has(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        if (m_storage[i] == key) {
            return true;
        }
    }
    return false;
}

void WeakSetObject::finalizer(Object* self, void* data)
{
    WeakSetObject* s = (WeakSetObject*)data;
    for (size_t i = 0; i < s->m_storage.size(); i++) {
        if (s->m_storage[i] == self) {
            s->m_storage.erase(i);
            break;
        }
    }
}
} // namespace Escargot
