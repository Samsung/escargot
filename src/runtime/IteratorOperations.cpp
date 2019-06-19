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
        func = Object::getMethod(state, obj, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator));
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
void iteratorClose(ExecutionState& state, const Value& iterator)
{
    ASSERT(iterator.isObject());

    Value returnFunction = iterator.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().stringReturn)).value(state, iterator.asObject());
    if (returnFunction.isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "return function is undefined");
    }

    Value innerResult = Object::call(state, returnFunction, iterator, 0, nullptr);
    if (!innerResult.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "result is not an object");
    }
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
