/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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
#include "IteratorOperations.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/Object.h"
#include "runtime/FunctionObject.h"
#include "runtime/ErrorObject.h"

namespace Escargot {

// https://www.ecma-international.org/ecma-262/6.0/#sec-getiterator
Value getIterator(ExecutionState& state, const Value& obj, const Value& method)
{
    Value func = method;
    if (method.isEmpty()) {
        func = Object::getMethod(state, obj, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
    }

    Value iterator = Object::call(state, func, obj, 0, nullptr);
    if (!iterator.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result is not an object");
    }

    return iterator;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-iteratornext
Value iteratorNext(ExecutionState& state, const Value& iterator, const Value& value)
{
    Object* obj = iterator.toObject(state);
    Value func = obj->get(state, ObjectPropertyName(state.context()->staticStrings().next)).value(state, obj);
    Value result;

    if (value.isEmpty()) {
        result = Object::call(state, func, iterator, 0, nullptr);
    } else {
        Value argumentList[] = { value };
        result = Object::call(state, func, iterator, 1, argumentList);
    }

    if (!result.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result is not an object");
    }
    return result;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-iteratorcomplete
bool iteratorComplete(ExecutionState& state, const Value& iterResult)
{
    ASSERT(iterResult.isObject());
    Value result = iterResult.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().done)).value(state, iterResult.asObject());

    return result.toBoolean(state);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-iteratorvalue
Value iteratorValue(ExecutionState& state, const Value& iterResult)
{
    ASSERT(iterResult.isObject());
    Value result = iterResult.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().value)).value(state, iterResult.asObject());

    return result;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-iteratorstep
Value iteratorStep(ExecutionState& state, const Value& iterator)
{
    Value result = iteratorNext(state, iterator, Value(Value::EmptyValue));
    bool done = iteratorComplete(state, result);

    return done ? Value(Value::False) : result;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-iteratorclose
Value iteratorClose(ExecutionState& state, const Value& iterator, const Value& completionValue, bool hasThrowOnCompletionType)
{
    ASSERT(iterator.isObject());

    // Let return be GetMethod(iterator, "return").
    // ReturnIfAbrupt(return).
    Value returnFunction = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().stringReturn)).value(state, iterator.asObject());
    // If return is undefined, return Completion(completion).
    if (returnFunction.isUndefined()) {
        if (hasThrowOnCompletionType) {
            state.throwException(completionValue);
        }
        return completionValue;
    }

    // Let innerResult be Call(return, iterator, «‍ »).
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

// https://www.ecma-international.org/ecma-262/6.0/#sec-createiterresultobject
Value createIterResultObject(ExecutionState& state, const Value& value, bool done)
{
    Object* obj = new Object(state);
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().done), ObjectPropertyDescriptor(Value(done), ObjectPropertyDescriptor::AllPresent));

    return obj;
}
}
