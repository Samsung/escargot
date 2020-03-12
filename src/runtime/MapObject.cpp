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
#include "MapObject.h"
#include "ArrayObject.h"
#include "Context.h"

namespace Escargot {

MapObject::MapObject(ExecutionState& state)
    : Object(state)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->mapPrototype());
}

MapObject::MapObject(ExecutionState& state, Object* proto)
    : Object(state)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, proto);
}

void* MapObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(MapObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapObject, m_storage));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(MapObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void MapObject::clear(ExecutionState& state)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        m_storage[i] = std::make_pair(Value(Value::EmptyValue), Value(Value::EmptyValue));
    }
}

size_t MapObject::size(ExecutionState& state)
{
    size_t siz = 0;
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i].first;
        if (existingKey.isEmpty()) {
            continue;
        }
        siz++;
    }
    return siz;
}

bool MapObject::deleteOperation(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i].first;
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            m_storage[i] = std::make_pair(Value(Value::EmptyValue), Value(Value::EmptyValue));
            return true;
        }
    }
    return false;
}

Value MapObject::get(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i].first;
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            return m_storage[i].second;
        }
    }
    return Value();
}

bool MapObject::has(ExecutionState& state, const Value& key)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i].first;
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            return true;
        }
    }
    return false;
}

void MapObject::set(ExecutionState& state, const Value& key, const Value& value)
{
    for (size_t i = 0; i < m_storage.size(); i++) {
        Value existingKey = m_storage[i].first;
        if (existingKey.isEmpty()) {
            continue;
        }
        if (existingKey.equalsToByTheSameValueZeroAlgorithm(state, key)) {
            m_storage[i].second = value;
            return;
        }
    }

    // If key is -0, let key be +0.
    if (key.isNumber() && key.asNumber() == 0 && std::signbit(key.asNumber())) {
        m_storage.pushBack(std::make_pair(Value(0), value));
    } else {
        m_storage.pushBack(std::make_pair(key, value));
    }
}

MapIteratorObject* MapObject::values(ExecutionState& state)
{
    return new MapIteratorObject(state, this, MapIteratorObject::TypeValue);
}

MapIteratorObject* MapObject::keys(ExecutionState& state)
{
    return new MapIteratorObject(state, this, MapIteratorObject::TypeKey);
}

MapIteratorObject* MapObject::entries(ExecutionState& state)
{
    return new MapIteratorObject(state, this, MapIteratorObject::TypeKeyValue);
}

MapIteratorObject::MapIteratorObject(ExecutionState& state, MapObject* map, Type type)
    : IteratorObject(state)
    , m_map(map)
    , m_iteratorIndex(0)
    , m_type(type)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->mapIteratorPrototype());
}

void* MapIteratorObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(MapIteratorObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapIteratorObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapIteratorObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapIteratorObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(MapIteratorObject, m_map));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(MapIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> MapIteratorObject::advance(ExecutionState& state)
{
    // Let m be the value of the [[Map]] internal slot of O.
    // Let index be the value of the [[MapNextIndex]] internal slot of O.
    // Let itemKind be the value of the [[MapIterationKind]] internal slot of O.
    MapObject* m = m_map;
    size_t index = m_iteratorIndex;
    Type itemKind = m_type;

    // If m is undefined, return CreateIterResultObject(undefined, true).
    if (m == nullptr) {
        return std::make_pair(Value(), true);
    }

    // Let entries be the List that is the value of the [[MapData]] internal slot of m.
    // Repeat while index is less than the total number of elements of entries. The number of elements must be redetermined each time this method is evaluated.
    while (index < m->m_storage.size()) {
        // Let e be the Record {[[Key]], [[Value]]} that is the value of entries[index].
        auto e = m->m_storage[index];
        // Set index to index+1.
        index++;
        // Set the [[MapNextIndex]] internal slot of O to index.
        m_iteratorIndex = index;

        if (e.first.isEmpty()) {
            continue;
        }
        // If e.[[Key]] is not empty, then
        // If itemKind is "key", let result be e.[[Key]].
        // Else if itemKind is "value", let result be e.[[Value]].
        // Else,
        // Assert: itemKind is "key+value".
        // Let result be CreateArrayFromList(« e.[[Key]], e.[[Value]] »).
        // Return CreateIterResultObject(result, false).
        Value result;
        if (itemKind == Type::TypeKey) {
            result = e.first;
        } else if (itemKind == Type::TypeValue) {
            result = e.second;
        } else if (itemKind == Type::TypeKeyValue) {
            ArrayObject* arr = new ArrayObject(state);
            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(0)), ObjectPropertyDescriptor(e.first, ObjectPropertyDescriptor::AllPresent));
            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(1)), ObjectPropertyDescriptor(e.second, ObjectPropertyDescriptor::AllPresent));
            result = arr;
        }
        return std::make_pair(result, false);
    }

    // Set the [[Map]] internal slot of O to undefined.
    m_map = nullptr;
    // Return CreateIterResultObject(undefined, true).
    return std::make_pair(Value(), true);
}
}
