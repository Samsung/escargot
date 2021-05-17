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
#include "runtime/SetObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/Context.h"

namespace Escargot {

SetObject::SetObject(ExecutionState& state)
    : SetObject(state, state.context()->globalObject()->setPrototype())
{
}

SetObject::SetObject(ExecutionState& state, Object* proto)
    : Object(state, proto)
{
}

void* SetObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetObject, m_storage));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void SetObject::clear(ExecutionState& state)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        m_storage[i] = Value(Value::EmptyValue);
    }
}

bool SetObject::deleteOperation(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i];
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            m_storage[i] = Value(Value::EmptyValue);
            return true;
        }
    }
    return false;
}

void SetObject::add(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i];
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            return;
        }
    }

    // If key is -0, let key be +0.
    if (key.isNumber() && key.asNumber() == 0 && std::signbit(key.asNumber())) {
        m_storage.pushBack(Value(0));
    } else {
        m_storage.pushBack(key);
    }
}

bool SetObject::has(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i];
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            return true;
        }
    }
    return false;
}

size_t SetObject::size(ExecutionState& state)
{
    size_t siz = 0;
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i];
        if (existingKey.isEmpty()) {
            continue;
        }
        siz++;
    }
    return siz;
}

IteratorObject* SetObject::values(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeValue);
}

IteratorObject* SetObject::keys(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeValue);
}

IteratorObject* SetObject::entries(ExecutionState& state)
{
    return new SetIteratorObject(state, this, SetIteratorObject::TypeKeyValue);
}

SetIteratorObject::SetIteratorObject(ExecutionState& state, SetObject* set, Type type)
    : IteratorObject(state, state.context()->globalObject()->setIteratorPrototype())
    , m_set(set)
    , m_iteratorIndex(0)
    , m_type(type)
{
}

void* SetIteratorObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SetIteratorObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SetIteratorObject, m_set));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SetIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> SetIteratorObject::advance(ExecutionState& state)
{
    // Let s be the value of the [[IteratedSet]] internal slot of O.
    // Let index be the value of the [[SetNextIndex]] internal slot of O.
    // Let itemKind be the value of the [[SetIterationKind]] internal slot of O.
    SetObject* s = m_set;
    size_t index = m_iteratorIndex;
    Type itemKind = m_type;

    // If s is undefined, return CreateIterResultObject(undefined, true).
    if (s == nullptr) {
        return std::make_pair(Value(), true);
    }

    // Let entries be the List that is the value of the [[SetData]] internal slot of s.
    // Repeat while index is less than the total number of elements of entries. The number of elements must be redetermined each time this method is evaluated.
    while (index < s->m_storage.size()) {
        // Let e be entries[index].
        Value e = s->m_storage[index];
        // Set index to index+1.
        index++;
        // Set the [[SetNextIndex]] internal slot of O to index.
        m_iteratorIndex = index;

        if (e.isEmpty()) {
            continue;
        }

        Value result;
        if (itemKind == Type::TypeKeyValue) {
            ArrayObject* arr = new ArrayObject(state);
            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(e, ObjectPropertyDescriptor::AllPresent));
            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(1)), ObjectPropertyDescriptor(e, ObjectPropertyDescriptor::AllPresent));
            result = arr;
        } else {
            result = e;
        }

        return std::make_pair(result, false);
    }

    // Set the [[IteratedSet]] internal slot of O to undefined.
    m_set = nullptr;
    // Return CreateIterResultObject(undefined, true).
    return std::make_pair(Value(), true);
}
} // namespace Escargot
