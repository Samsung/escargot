/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    setPrototype(state, state.context()->globalObject()->stringPrototype());
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


ObjectGetResult StringObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    Value::ValueIndex idx;
    if (P.isUIntType()) {
        idx = P.uintValue();
    } else {
        idx = P.string(state)->tryToUseAsIndex();
    }
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

void StringObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    size_t len = m_primitiveValue->length();
    for (size_t i = 0; i < len; i++) {
        if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::EnumerablePresent), data)) {
            return;
        }
    }
    Object::enumeration(state, callback, data);
}

ObjectGetResult StringObject::getIndexedProperty(ExecutionState& state, const Value& property)
{
    Value::ValueIndex idx;
    if (LIKELY(property.isUInt32())) {
        idx = property.asUInt32();
    } else {
        idx = property.toString(state)->tryToUseAsIndex();
    }
    if (idx != Value::InvalidIndexValue) {
        size_t strLen = m_primitiveValue->length();
        if (LIKELY(idx < strLen)) {
            return ObjectGetResult(Value(String::fromCharCode(m_primitiveValue->charAt(idx))), false, true, false);
        }
    }
    return get(state, ObjectPropertyName(state, property));
}

IteratorObject* StringObject::iterator(ExecutionState& state)
{
    return new StringIteratorObject(state, m_primitiveValue);
}

StringIteratorObject::StringIteratorObject(ExecutionState& state, String* s)
    : IteratorObject(state)
    , m_string(s)
    , m_iteratorNextIndex(0)
{
    setPrototype(state, state.context()->globalObject()->stringIteratorPrototype());
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
        resultString = String::fromInt32(first);
    } else {
        // Let second be the code unit value at index position+1 in the String S.
        auto second = s->charAt(position + 1);
        // If second < 0xDC00 or second > 0xDFFF, let resultString be the string consisting of the single code unit first.
        if (second < 0xDC00 || second > 0xDFFF) {
            resultString = String::fromInt32(first);
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
