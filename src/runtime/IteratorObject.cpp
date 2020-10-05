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
    : Object(state, proto)
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result of GetIterator is not an object");
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result is not an object");
    }

    return result.asObject();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorcomplete
bool IteratorObject::iteratorComplete(ExecutionState& state, Object* iterResult)
{
    Value result = iterResult->get(state, ObjectPropertyName(state.context()->staticStrings().done)).value(state, iterResult);
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
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Iterator close result is not an object");
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
}
