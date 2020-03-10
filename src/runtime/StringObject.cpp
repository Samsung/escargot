/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "StringObject.h"
#include "Context.h"

namespace Escargot {

StringObject::StringObject(ExecutionState& state, String* value)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1, true)
    , m_primitiveValue(value)
{
    m_structure = state.context()->defaultStructureForStringObject();
    m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER] = Value();
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->stringPrototype());
}

void* StringObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(StringObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringObject, m_primitiveValue));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(StringObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ObjectHasPropertyResult StringObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    Value::ValueIndex idx = P.tryToUseAsIndex();
    if (idx != Value::InvalidIndexValue) {
        size_t strLen = m_primitiveValue->length();
        if (LIKELY(idx < strLen)) {
            return ObjectHasPropertyResult(ObjectGetResult(Value(String::fromCharCode(m_primitiveValue->charAt(idx))), false, true, false));
        }
    }
    return Object::hasProperty(state, P);
}


ObjectGetResult StringObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    Value::ValueIndex idx = P.tryToUseAsIndex();
    if (idx != Value::InvalidIndexValue) {
        size_t strLen = m_primitiveValue->length();
        if (LIKELY(idx < strLen)) {
            return ObjectGetResult(Value(String::fromCharCode(m_primitiveValue->charAt(idx))), false, true, false);
        }
    }
    return Object::getOwnProperty(state, P);
}

bool StringObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    auto r = getOwnProperty(state, P);
    if (r.hasValue() && !r.isConfigurable())
        return false;
    return Object::defineOwnProperty(state, P, desc);
}

bool StringObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    auto r = getOwnProperty(state, P);
    if (r.hasValue() && !r.isConfigurable())
        return false;
    return Object::deleteOwnProperty(state, P);
}

void StringObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    size_t len = m_primitiveValue->length();
    for (size_t i = 0; i < len; i++) {
        if (!callback(state, this, ObjectPropertyName(state, i), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::EnumerablePresent), data)) {
            return;
        }
    }
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);
}

ObjectGetResult StringObject::getIndexedProperty(ExecutionState& state, const Value& property)
{
    Value::ValueIndex idx = property.tryToUseAsIndex(state);
    if (idx != Value::InvalidIndexValue) {
        size_t strLen = m_primitiveValue->length();
        if (LIKELY(idx < strLen)) {
            return ObjectGetResult(Value(String::fromCharCode(m_primitiveValue->charAt(idx))), false, true, false);
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

ObjectHasPropertyResult StringObject::hasIndexedProperty(ExecutionState& state, const Value& propertyName)
{
    Value::ValueIndex idx = propertyName.tryToUseAsIndex(state);
    if (idx != Value::InvalidIndexValue) {
        size_t strLen = m_primitiveValue->length();
        if (LIKELY(idx < strLen)) {
            return ObjectHasPropertyResult(ObjectGetResult(Value(String::fromCharCode(m_primitiveValue->charAt(idx))), false, true, false));
        }
    }
    return hasProperty(state, ObjectPropertyName(state, propertyName));
}

StringIteratorObject::StringIteratorObject(ExecutionState& state, String* s)
    : IteratorObject(state)
    , m_string(s)
    , m_iteratorNextIndex(0)
{
    Object::setPrototypeForIntrinsicObjectCreation(state, state.context()->globalObject()->stringIteratorPrototype());
}

void* StringIteratorObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(StringIteratorObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringIteratorObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringIteratorObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringIteratorObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(StringIteratorObject, m_string));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(StringIteratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> StringIteratorObject::advance(ExecutionState& state)
{
    // Let s be the value of the [[IteratedString]] internal slot of O.
    String* s = m_string;
    // If a is undefined, return CreateIterResultObject(undefined, true).
    if (s == nullptr) {
        return std::make_pair(Value(), true);
    }
    // Let position be the value of the [[StringIteratorNextIndex]] internal slot of O.
    size_t position = m_iteratorNextIndex;
    // Let len be the number of elements in s.
    auto len = s->length();

    // If position ≥ len, then
    if (position >= len) {
        // Set the value of the [[IteratedString]] internal slot of O to undefined.
        m_string = nullptr;
        // Return CreateIterResultObject(undefined, true).
        return std::make_pair(Value(), true);
    }

    // Let first be the code unit value at index position in s.
    auto first = s->charAt(position);
    // If first < 0xD800 or first > 0xDBFF or position+1 = len, let resultString be the string consisting of the single code unit first.
    String* resultString;
    if (first < 0xD800 || first > 0xDBFF || (position + 1 == len)) {
        resultString = String::fromCharCode(first);
    } else {
        // Let second be the code unit value at index position+1 in the String S.
        auto second = s->charAt(position + 1);
        // If second < 0xDC00 or second > 0xDFFF, let resultString be the string consisting of the single code unit first.
        if (second < 0xDC00 || second > 0xDFFF) {
            resultString = String::fromCharCode(first);
        } else {
            // Else, let resultString be the string consisting of the code unit first followed by the code unit second.
            char16_t s[2] = { first, second };
            resultString = new UTF16String(s, 2);
        }
    }
    // Let resultSize be the number of code units in resultString.
    auto resultSize = resultString->length();

    // Set the value of the [[StringIteratorNextIndex]] internal slot of O to position + resultSize.
    m_iteratorNextIndex = position + resultSize;

    // Return CreateIterResultObject(resultString, false).
    return std::make_pair(Value(resultString), false);
}
}
