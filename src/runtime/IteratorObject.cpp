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
#include "IteratorObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/AsyncFromSyncIteratorObject.h"
#include "runtime/ScriptAsyncFunctionObject.h"
#include "runtime/StringObject.h"

namespace Escargot {

IteratorObject::IteratorObject(ExecutionState& state)
    : IteratorObject(state, state.context()->globalObject()->objectPrototype())
{
}

IteratorObject::IteratorObject(ExecutionState& state, Object* proto)
    : DerivedObject(state, proto)
{
}

Value IteratorObject::next(ExecutionState& state)
{
    auto result = advance(state);
    Object* r = new Object(state);

    r->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(result.first, ObjectPropertyDescriptor::AllPresent));
    r->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().done), ObjectPropertyDescriptor(Value(result.second), ObjectPropertyDescriptor::AllPresent));
    return r;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-getiterator
IteratorRecord* IteratorObject::getIterator(ExecutionState& state, const Value& obj, const bool sync, const Value& func)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    Value method = func;
    // If method is not present, then
    if (method.isEmpty()) {
        // If hint is async, then
        if (!sync) {
            // Set method to ? GetMethod(obj, @@asyncIterator).
            method = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().asyncIterator));
            // If method is undefined, then
            if (method.isUndefined()) {
                // Let syncMethod be ? GetMethod(obj, @@iterator).
                auto syncMethod = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
                // Let syncIteratorRecord be ? GetIterator(obj, sync, syncMethod).
                auto syncIteratorRecord = getIterator(state, obj, true, syncMethod);
                // Return ? CreateAsyncFromSyncIterator(syncIteratorRecord).
                return AsyncFromSyncIteratorObject::createAsyncFromSyncIterator(state, syncIteratorRecord);
            }
        } else {
            // Otherwise, set method to ? GetMethod(obj, @@iterator).
            method = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
        }
    }

    // Let iterator be ? Call(method, obj).
    Value iterator = Object::call(state, method, obj, 0, nullptr);

    // If Type(iterator) is not Object, throw a TypeError exception.
    if (!iterator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "result of GetIterator is not an object");
    }

    // Let nextMethod be ? GetV(iterator, "next").
    Value nextMethod = iterator.asObject()->get(state, ObjectPropertyName(strings->next)).value(state, iterator);

    // Let iteratorRecord be Record { [[Iterator]]: iterator, [[NextMethod]]: nextMethod, [[Done]]: false }.
    // Return iteratorRecord
    return new IteratorRecord(iterator.asObject(), nextMethod, false);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratornext
Object* IteratorObject::iteratorNext(ExecutionState& state, IteratorRecord* iteratorRecord, const Value& value)
{
    auto strings = &state.context()->staticStrings();

    IteratorRecord* record = iteratorRecord;
    Value result;
    Value nextMethod = record->m_nextMethod;
    Value iterator = record->m_iterator;
    if (value.isEmpty()) {
        result = Object::call(state, nextMethod, iterator, 0, nullptr);
    } else {
        Value args[] = { value };
        result = Object::call(state, nextMethod, iterator, 1, args);
    }

    if (!result.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "result is not an object");
    }

    return result.asObject();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorcomplete
bool IteratorObject::iteratorComplete(ExecutionState& state, Object* iterResult)
{
    Value result = iterResult->get(state, ObjectPropertyName(state.context()->staticStrings().done)).value(state, iterResult);
    return result.toBoolean();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorvalue
Value IteratorObject::iteratorValue(ExecutionState& state, Object* iterResult)
{
    return iterResult->get(state, ObjectPropertyName(state.context()->staticStrings().value)).value(state, iterResult);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorstep
Optional<Object*> IteratorObject::iteratorStep(ExecutionState& state, IteratorRecord* iteratorRecord)
{
    Object* result = IteratorObject::iteratorNext(state, iteratorRecord);
    bool done = IteratorObject::iteratorComplete(state, result);

    return done ? nullptr : result;
}

// https://tc39.es/ecma262/#sec-iteratorstepvalue
Optional<Value> IteratorObject::iteratorStepValue(ExecutionState& state, IteratorRecord* iteratorRecord)
{
    // Let result be ? IteratorStep(iteratorRecord).
    auto result = iteratorStep(state, iteratorRecord);
    // If result is done, then
    if (!result) {
        // Return done.
        return NullOption;
    }

    Value value;
    // Let value be Completion(IteratorValue(result)).
    try {
        value = iteratorValue(state, result.value());
    } catch (const Value& e) {
        // If value is a throw completion, then
        // Set iteratorRecord.[[Done]] to true.
        iteratorRecord->m_done = true;
        throw e;
    }
    // Return ? value.
    return value;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorclose
Value IteratorObject::iteratorClose(ExecutionState& state, IteratorRecord* iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType)
{
    auto strings = &state.context()->staticStrings();

    IteratorRecord* record = iteratorRecord;
    Value iterator = record->m_iterator;
    Value returnFunction = Object::getMethod(state, iterator, ObjectPropertyName(strings->stringReturn));
    if (returnFunction.isUndefined()) {
        if (hasThrowOnCompletionType) {
            state.throwException(completionValue);
        }
        return completionValue;
    }

    // Let innerResult be Call(return, iterator, « »).
    Value innerResult;
    bool innerResultHasException = false;
    try {
        innerResult = Object::call(state, returnFunction, iterator, 0, nullptr);
    } catch (const Value& e) {
        innerResult = e;
        innerResultHasException = true;
    }
    // If completion.[[type]] is throw, return Completion(completion).
    if (hasThrowOnCompletionType) {
        state.throwException(completionValue);
    }
    // If innerResult.[[type]] is throw, return Completion(innerResult).
    if (innerResultHasException) {
        state.throwException(innerResult);
    }
    // If Type(innerResult.[[value]]) is not Object, throw a TypeError exception.
    if (!innerResult.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Iterator close result is not an object");
    }
    // Return Completion(completion).
    return completionValue;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-createiterresultobject
Object* IteratorObject::createIterResultObject(ExecutionState& state, const Value& value, bool done)
{
    Object* obj = new Object(state);
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().done), ObjectPropertyDescriptor(Value(done), ObjectPropertyDescriptor::AllPresent));

    return obj;
}

ValueVectorWithInlineStorage IteratorObject::iterableToList(ExecutionState& state, const Value& items, Optional<Value> method)
{
    IteratorRecord* iteratorRecord;
    if (method.hasValue()) {
        iteratorRecord = IteratorObject::getIterator(state, items, true, method.value());
    } else {
        iteratorRecord = IteratorObject::getIterator(state, items, true);
    }
    ValueVectorWithInlineStorage values;
    Optional<Object*> next;

    while (true) {
        next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (next.hasValue()) {
            Value nextValue = IteratorObject::iteratorValue(state, next.value());
            values.pushBack(nextValue);
        } else {
            break;
        }
    }

    return values;
}

/*13.1 IterableToListOfType*/
ValueVector IteratorObject::iterableToListOfType(ExecutionState& state, const Value items, const String* elementTypes)
{
    IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, items, true);
    ValueVector values;
    Optional<Object*> next;

    while (true) {
        next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (!next.hasValue()) {
            break;
        }
        Value nextValue = IteratorObject::iteratorValue(state, next->asObject());
        if (!nextValue.isString()) {
            Value throwCompletion = ErrorObject::createError(state, ErrorCode::RangeError, new ASCIIStringFromExternalMemory("Got invalid value"));
            return { IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true) };
        }
        values.pushBack(nextValue);
    }
    return values;
}

// https://tc39.es/ecma262/multipage/abstract-operations.html#sec-add-value-to-keyed-group
static void addValueToKeyedGroup(ExecutionState& state, IteratorObject::KeyedGroupVector& groups, const Value& key, const Value& value)
{
    // For each Record { [[Key]], [[Elements]] } g of groups, do
    for (size_t i = 0; i < groups.size(); i++) {
        // If SameValue(g.[[Key]], key) is true, then
        if (groups[i]->key.equalsToByTheSameValueAlgorithm(state, key)) {
            // Assert: Exactly one element of groups meets this criterion.
            // Append value to g.[[Elements]].
            groups[i]->elements.pushBack(value);
            // Return UNUSED.
            return;
        }
    }
    // Let group be the Record { [[Key]]: key, [[Elements]]: « value » }.
    // Append group to groups.
    IteratorObject::KeyedGroup* kg = new IteratorObject::KeyedGroup();
    kg->key = key;
    kg->elements.pushBack(value);
    groups.pushBack(kg);
    // Return UNUSED.
    return;
}

IteratorObject::KeyedGroupVector IteratorObject::groupBy(ExecutionState& state, const Value& items, const Value& callbackfn, GroupByKeyCoercion keyCoercion)
{
    // Perform ? RequireObjectCoercible(items).
    if (items.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "GroupBy called on undefined or null");
    }
    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "GroupBy called on non-callable value");
    }
    // Let groups be a new empty List.
    KeyedGroupVector groups;
    // Let iteratorRecord be ? GetIterator(items, SYNC).
    IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, items, true);
    // Let k be 0.
    int64_t k = 0;

    // Repeat,
    while (true) {
        // a. If k ≥ 2**53 - 1, then
        if (k >= ((1LL << 53LL) - 1LL)) {
            // Let error be ThrowCompletion(a newly created TypeError object).
            Value error = ErrorObject::createError(state, ErrorCode::TypeError, new ASCIIStringFromExternalMemory("Too many result on GroupBy function"));
            // Return ? IteratorClose(iteratorRecord, error).
            IteratorObject::iteratorClose(state, iteratorRecord, error, true);
            ASSERT_NOT_REACHED();
        }

        // Let next be ? IteratorStepValue(iteratorRecord).
        Optional<Object*> next = IteratorObject::iteratorStep(state, iteratorRecord);
        // If next is DONE, then
        if (!next) {
            // Return groups.
            return groups;
        }

        // Let value be next.
        Value value = IteratorObject::iteratorValue(state, next.value());
        // Let key be Completion(Call(callbackfn, undefined, « value, 𝔽(k) »)).
        Value key;
        try {
            Value argv[2] = { value, Value(k) };
            key = Object::call(state, callbackfn, Value(), 2, argv);
        } catch (const Value& error) {
            // IfAbruptCloseIterator(key, iteratorRecord).
            IteratorObject::iteratorClose(state, iteratorRecord, error, true);
            ASSERT_NOT_REACHED();
        }
        // If keyCoercion is PROPERTY, then
        if (keyCoercion == GroupByKeyCoercion::Property) {
            // Set key to Completion(ToPropertyKey(key)).
            try {
                key = key.toPropertyKey(state);
            } catch (const Value& error) {
                // IfAbruptCloseIterator(key, iteratorRecord).
                IteratorObject::iteratorClose(state, iteratorRecord, error, true);
                ASSERT_NOT_REACHED();
            }
        } else {
            // Assert: keyCoercion is COLLECTION.
            ASSERT(keyCoercion == GroupByKeyCoercion::Collection);
            // Set key to CanonicalizeKeyedCollectionKey(key).
            key = key.toCanonicalizeKeyedCollectionKey(state);
        }
        // Perform AddValueToKeyedGroup(groups, key, value).
        addValueToKeyedGroup(state, groups, key, value);
        // Set k to k + 1.
        k = k + 1;
    }
}

// https://tc39.es/proposal-iterator-helpers/#sec-getiteratordirect
IteratorRecord* IteratorObject::getIteratorDirect(ExecutionState& state, Object* obj)
{
    // Let nextMethod be ? Get(obj, "next").
    Value nextMethod = obj->get(state, ObjectPropertyName(state.context()->staticStrings().next)).value(state, obj);
    // Let iteratorRecord be Record { [[Iterator]]: obj, [[NextMethod]]: nextMethod, [[Done]]: false }.
    IteratorRecord* record = new IteratorRecord(obj, nextMethod, false);
    // Return iteratorRecord.
    return record;
}

// https://tc39.es/ecma262/#sec-getiteratorfrommethod
IteratorRecord* IteratorObject::getIteratorFromMethod(ExecutionState& state, const Value& obj, const Value& method)
{
    // 1. Let iterator be ? Call(method, obj).
    Value iterator = Object::call(state, method, obj, 0, nullptr);
    // 2. If iterator is not an Object, throw a TypeError exception.
    if (!iterator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "iterator is not object");
    }
    // 3. Return ? GetIteratorDirect(iterator).
    return getIteratorDirect(state, iterator.asObject());
}

// https://tc39.es/ecma262/#sec-getiteratorflattenable
IteratorRecord* IteratorObject::getIteratorFlattenable(ExecutionState& state, const Value& obj, PrimitiveHandling primitiveHandling)
{
    // If obj is not an Object, then
    if (!obj.isObject()) {
        // If primitiveHandling is reject-primitives, throw a TypeError exception.
        if (primitiveHandling == PrimitiveHandling::RejectPrimitives) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "GetIteratorFlattenable called on non-object with reject-primitives");
        }
        // Assert: primitiveHandling is iterate-string-primitives.
        ASSERT(primitiveHandling == PrimitiveHandling::IterateStringPrimitives);
        // If obj is not a String, throw a TypeError exception.
        if (!obj.isString()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "GetIteratorFlattenable called on non-string primitive with iterate-string-primitives");
        }
    }

    // Let method be ? GetMethod(obj, %Symbol.iterator%).
    Value method = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
    Value iterator;

    // If method is undefined, then
    if (method.isUndefined()) {
        // Let iterator be obj.
        iterator = obj;
    } else {
        // Let iterator be ? Call(method, obj).
        iterator = Object::call(state, method, obj, 0, nullptr);
    }

    // If iterator is not an Object, throw a TypeError exception.
    if (!iterator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "GetIteratorFlattenable: iterator is not an object");
    }

    // Return ? GetIteratorDirect(iterator).
    return getIteratorDirect(state, iterator.asObject());
}

IteratorHelperObject::IteratorHelperObject(ExecutionState& state, IteratorHelperObjectCallback callback,
                                           IteratorRecord* underlyingIterator, void* data)
    : IteratorObject(state, state.context()->globalObject()->iteratorHelperPrototype())
    , m_isRunning(false)
    , m_callback(callback)
    , m_underlyingIterator(underlyingIterator)
    , m_data(data)
{
}

void* IteratorHelperObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(IteratorHelperObject)] = { 0 };
        IteratorObject::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(IteratorHelperObject, m_data));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(IteratorHelperObject, m_underlyingIterator));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(IteratorHelperObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

std::pair<Value, bool> IteratorHelperObject::advance(ExecutionState& state)
{
    struct IteratorHelperObjectRunningStateChanger {
        IteratorHelperObject& obj;
        IteratorHelperObjectRunningStateChanger(IteratorHelperObject& obj)
            : obj(obj)
        {
            ASSERT(!obj.m_isRunning);
            obj.m_isRunning = true;
        }
        ~IteratorHelperObjectRunningStateChanger()
        {
            obj.m_isRunning = false;
        }
    };

    if (m_isRunning) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "You cannot call Iterator helper advance function recursively");
    }

    IteratorHelperObjectRunningStateChanger changeRunningState(*this);
    return m_callback(state, this, m_data);
}

} // namespace Escargot
