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
#include "WeakMapObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

WeakMapObject::WeakMapObject(ExecutionState& state)
    : WeakMapObject(state, state.context()->globalObject()->weakMapPrototype())
{
}

WeakMapObject::WeakMapObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
    addFinalizer([](Object* self, void* data) {
        auto wm = self->asWeakMapObject();
        for (size_t i = 0; i < wm->m_storage.size(); i++) {
            wm->m_storage[i]->key->removeFinalizer(WeakMapObject::finalizer, wm);
        }
        wm->m_storage.clear();
    },
                 nullptr);
}

void* WeakMapObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(WeakMapObject)] = { 0 };
        Object::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(WeakMapObject, m_storage));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(WeakMapObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void* WeakMapObject::WeakMapObjectDataItem::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(WeakMapObject::WeakMapObjectDataItem)] = { 0 };
        GC_set_bit(desc, GC_WORD_OFFSET(WeakMapObject::WeakMapObjectDataItem, data));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(WeakMapObject::WeakMapObjectDataItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<WeakMapObject::WeakMapObjectDataItem>().allocate(1);
#endif
}

bool WeakMapObject::deleteOperation(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            key->removeFinalizer(finalizer, this);
            m_storage.erase(i);
            return true;
        }
    }
    return false;
}

Value WeakMapObject::get(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            return m_storage[i]->data;
        }
    }
    return Value();
}

bool WeakMapObject::has(ExecutionState& state, Object* key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            return true;
        }
    }
    return false;
}


void WeakMapObject::set(ExecutionState& state, Object* key, const Value& value)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        auto existingKey = m_storage[i]->key;
        if (existingKey == key) {
            m_storage[i]->data = value;
            return;
        }
    }

    auto newData = new WeakMapObjectDataItem();
    newData->key = key;
    newData->data = value;
    m_storage.pushBack(newData);

    key->addFinalizer(WeakMapObject::finalizer, this);
}

void WeakMapObject::finalizer(Object* self, void* data)
{
    WeakMapObject* s = (WeakMapObject*)data;
    for (size_t i = 0; i < s->m_storage.size(); i++) {
        if (s->m_storage[i]->key == self) {
            s->m_storage.erase(i);
            break;
        }
    }
}
} // namespace Escargot
