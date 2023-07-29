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
    RETURN_VALUE_IF_PENDING_EXCEPTION
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
            RETURN_NULL_IF_PENDING_EXCEPTION
            // If method is undefined, then
            if (method.isUndefined()) {
                // Let syncMethod be ? GetMethod(obj, @@iterator).
                auto syncMethod = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
                RETURN_NULL_IF_PENDING_EXCEPTION
                // Let syncIteratorRecord be ? GetIterator(obj, sync, syncMethod).
                auto syncIteratorRecord = getIterator(state, obj, true, syncMethod);
                RETURN_NULL_IF_PENDING_EXCEPTION
                // Return ? CreateAsyncFromSyncIterator(syncIteratorRecord).
                return AsyncFromSyncIteratorObject::createAsyncFromSyncIterator(state, syncIteratorRecord);
            }
        } else {
            // Otherwise, set method to ? GetMethod(obj, @@iterator).
            method = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
            RETURN_NULL_IF_PENDING_EXCEPTION
        }
    }

    // Let iterator be ? Call(method, obj).
    Value iterator = Object::call(state, method, obj, 0, nullptr);
    RETURN_NULL_IF_PENDING_EXCEPTION

    // If Type(iterator) is not Object, throw a TypeError exception.
    if (!iterator.isObject()) {
        THROW_BUILTIN_ERROR_RETURN_NULL(state, ErrorCode::TypeError, "result of GetIterator is not an object");
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
    RETURN_NULL_IF_PENDING_EXCEPTION

    if (!result.isObject()) {
        THROW_BUILTIN_ERROR_RETURN_NULL(state, ErrorCode::TypeError, "result is not an object");
    }

    return result.asObject();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorcomplete
bool IteratorObject::iteratorComplete(ExecutionState& state, Object* iterResult)
{
    Value result = iterResult->get(state, ObjectPropertyName(state.context()->staticStrings().done)).value(state, iterResult);
    RETURN_ZERO_IF_PENDING_EXCEPTION
    return result.toBoolean(state);
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
    RETURN_NULL_IF_PENDING_EXCEPTION
    bool done = IteratorObject::iteratorComplete(state, result);

    return done ? nullptr : result;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorclose
Value IteratorObject::iteratorClose(ExecutionState& state, IteratorRecord* iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType)
{
    auto strings = &state.context()->staticStrings();

    IteratorRecord* record = iteratorRecord;
    Value iterator = record->m_iterator;
    Value returnFunction = Object::getMethod(state, iterator, ObjectPropertyName(strings->stringReturn));
    RETURN_VALUE_IF_PENDING_EXCEPTION
    if (returnFunction.isUndefined()) {
        if (hasThrowOnCompletionType) {
            THROW_EXCEPTION_RETURN_VALUE(state, completionValue);
        }
        return completionValue;
    }

    // Let innerResult be Call(return, iterator, « »).
    bool innerResultHasException = false;
    Value innerResult = Object::call(state, returnFunction, iterator, 0, nullptr);
    if (UNLIKELY(state.hasPendingException())) {
        innerResult = state.detachPendingException();
        innerResultHasException = true;
    }
    // If completion.[[type]] is throw, return Completion(completion).
    if (hasThrowOnCompletionType) {
        THROW_EXCEPTION_RETURN_VALUE(state, completionValue);
    }
    // If innerResult.[[type]] is throw, return Completion(innerResult).
    if (innerResultHasException) {
        THROW_EXCEPTION_RETURN_VALUE(state, innerResult);
    }
    // If Type(innerResult.[[value]]) is not Object, throw a TypeError exception.
    if (!innerResult.isObject()) {
        THROW_BUILTIN_ERROR_RETURN_VALUE(state, ErrorCode::TypeError, "Iterator close result is not an object");
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

    if (UNLIKELY(state.hasPendingException())) {
        return values;
    }

    while (true) {
        next = IteratorObject::iteratorStep(state, iteratorRecord);
        if (UNLIKELY(state.hasPendingException())) {
            return values;
        }
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
    // ignore exception
    ASSERT(!state.hasPendingException());

    while (true) {
        next = IteratorObject::iteratorStep(state, iteratorRecord);
        ASSERT(!state.hasPendingException());
        if (!next.hasValue()) {
            break;
        }
        Value nextValue = IteratorObject::iteratorValue(state, next->asObject());
        if (!nextValue.isString()) {
            Value throwCompletion = ErrorObject::createError(state, ErrorCode::RangeError, new ASCIIString("Got invalid value"));
            return { IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true) };
        }
        values.pushBack(nextValue);
    }
    return values;
}

} // namespace Escargot
