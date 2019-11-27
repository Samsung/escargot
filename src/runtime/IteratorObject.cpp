/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

namespace Escargot {

IteratorObject::IteratorObject(ExecutionState& state)
    : Object(state)
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
Value IteratorObject::getIterator(ExecutionState& state, const Value& obj, const bool sync, const Value& func)
{
    Value method = func;
    if (method.isEmpty()) {
        if (sync) {
            method = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
        } else {
            // async iterator is not yet supported
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    Value iterator = Object::call(state, method, obj, 0, nullptr);
    if (!iterator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result of GetIterator is not an object");
    }

    Value nextMethod = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().next)).value(state, obj);
    return new IteratorRecord(iterator, nextMethod, false);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratornext
Value IteratorObject::iteratorNext(ExecutionState& state, const Value& iteratorRecord, const Value& value)
{
    ASSERT(iteratorRecord.isPointerValue() && iteratorRecord.asPointerValue()->isIteratorRecord());

    Value result;
    IteratorRecord* record = iteratorRecord.asPointerValue()->asIteratorRecord();
    if (value.isEmpty()) {
        result = Object::call(state, record->m_nextMethod, record->m_iterator, 0, nullptr);
    } else {
        Value args[] = { value };
        result = Object::call(state, record->m_nextMethod, record->m_iterator, 1, args);
    }

    if (!result.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result is not an object");
    }

    return result;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorcomplete
bool IteratorObject::iteratorComplete(ExecutionState& state, const Value& iterResult)
{
    ASSERT(iterResult.isObject());
    Value result = iterResult.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().done)).value(state, iterResult);

    return result.toBoolean(state);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorvalue
Value IteratorObject::iteratorValue(ExecutionState& state, const Value& iterResult)
{
    ASSERT(iterResult.isObject());
    return iterResult.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().value)).value(state, iterResult);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorstep
Value IteratorObject::iteratorStep(ExecutionState& state, const Value& iteratorRecord)
{
    Value result = IteratorObject::iteratorNext(state, iteratorRecord);
    bool done = IteratorObject::iteratorComplete(state, result);

    return done ? Value(Value::False) : result;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-iteratorclose
Value IteratorObject::iteratorClose(ExecutionState& state, const Value& iteratorRecord, const Value& completionValue, bool hasThrowOnCompletionType)
{
    ASSERT(iteratorRecord.isPointerValue() && iteratorRecord.asPointerValue()->isIteratorRecord());

    IteratorRecord* record = iteratorRecord.asPointerValue()->asIteratorRecord();

    Value iterator = record->m_iterator;
    Value returnFunction = Object::getMethod(state, iterator, ObjectPropertyName(state.context()->staticStrings().stringReturn));
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
Value IteratorObject::createIterResultObject(ExecutionState& state, const Value& value, bool done)
{
    Object* obj = new Object(state);
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().done), ObjectPropertyDescriptor(Value(done), ObjectPropertyDescriptor::AllPresent));

    return obj;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-createlistiteratorRecord
Value IteratorObject::createListIteratorRecord(ExecutionState& state, const Value& list)
{
    // createListIteratorRecord is not yet supported
    RELEASE_ASSERT_NOT_REACHED();
}
}
