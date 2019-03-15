/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "ArrayObject.h"
#include "IteratorOperations.h"
#include "ToStringRecursionPreventer.h"

namespace Escargot {

Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    bool interpretArgumentsAsElements = false;
    size_t size = 0;
    if (argc > 1) {
        size = argc;
        interpretArgumentsAsElements = true;
    } else if (argc == 1) {
        Value& val = argv[0];
        if (val.isNumber()) {
            if (val.equalsTo(state, Value(val.toUint32(state)))) {
                size = val.toNumber(state);
            } else {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
            }
        } else {
            size = 1;
            interpretArgumentsAsElements = true;
        }
    }
    ArrayObject* array;
    if (isNewExpression && thisValue.isObject() && thisValue.asObject()->isArrayObject()) {
        array = thisValue.asPointerValue()->asObject()->asArrayObject();
    } else {
        array = new ArrayObject(state);
    }

    array->setArrayLength(state, size);

    if (interpretArgumentsAsElements) {
        Value val = argv[0];
        if (argc > 1 || !val.isInt32()) {
            if (array->isFastModeArray()) {
                for (size_t idx = 0; idx < argc; idx++) {
                    array->m_fastModeData[idx] = argv[idx];
                }
            } else {
                for (size_t idx = 0; idx < argc; idx++) {
                    array->ArrayObject::defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), ObjectPropertyDescriptor(val, ObjectPropertyDescriptor::AllPresent));
                    val = argv[idx + 1];
                }
            }
        }
    }
    return array;
}

static Value builtinArrayIsArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isObject())
        return Value(false);
    if (argv[0].asObject()->isArrayObject())
        return Value(true);
    else
        return Value(false);
}

static Value builtinArrayFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Array.from ( items [ , mapfn [ , thisArg ] ] )#
    Value items = argv[0];
    Value mapfn;
    if (argc > 1) {
        mapfn = argv[1];
    }
    Value thisArg;
    if (argc > 2) {
        thisArg = argv[2];
    }
    // Let C be the this value.
    Value C = thisValue;
    Value T;
    // If mapfn is undefined, let mapping be false.
    bool mapping = false;
    if (!mapfn.isUndefined()) {
        // If IsCallable(mapfn) is false, throw a TypeError exception.
        if (!mapfn.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "argument map function should be undefined or function");
        }
        // If thisArg was supplied, let T be thisArg; else let T be undefined.
        T = thisArg;
        // Let mapping be true.
        mapping = true;
    }

    // Let usingIterator be ? GetMethod(items, @@iterator).
    items = items.toObject(state);
    Value usingIterator = items.asObject()->get(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator)).value(state, items);
    // If usingIterator is not undefined, then
    if (!usingIterator.isUndefined()) {
        Object* A;
        // If IsConstructor(C) is true, then
        if (C.isFunction() && C.asFunction()->isConstructor()) {
            // Let A be ? Construct(C).
            A = C.asFunction()->newInstance(state, 0, nullptr);
        } else {
            // Let A be ArrayCreate(0).
            A = new ArrayObject(state);
        }
        // Let iterator be ? GetIterator(items, usingIterator).
        Value iterator = getIterator(state, items, usingIterator);
        // Let k be 0.
        double k = 0;
        // Repeat
        while (true) {
            // If k ≥ 2^53-1, then
            if (k >= ((1LL << 53LL) - 1LL)) {
                // Let error be Completion{[[Type]]: throw, [[Value]]: a newly created TypeError object, [[Target]]: empty}.
                // Return ? IteratorClose(iterator, error).
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Got invalid index");
            }
            // Let Pk be ! ToString(k).
            ObjectPropertyName pk(state, Value(k));
            // Let next be ? IteratorStep(iterator).
            Value next = iteratorStep(state, iterator);
            // If next is false, then
            if (next.isFalse()) {
                // Perform ? Set(A, "length", k, true).
                A->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().length), Value(k), A);
                // Return A.
                return A;
            }
            // Let nextValue be ? IteratorValue(next).
            Value nextValue = iteratorValue(state, next);
            Value mappedValue;
            // If mapping is true, then
            if (mapping) {
                // Let mappedValue be Call(mapfn, T, « nextValue, k »).
                // If mappedValue is an abrupt completion, return ? IteratorClose(iterator, mappedValue).
                // Let mappedValue be mappedValue.[[Value]].
                Value argv[] = { nextValue, Value(k) };
                mappedValue = mapfn.asFunction()->call(state, T, 2, argv);
            } else {
                mappedValue = nextValue;
            }
            // Let defineStatus be CreateDataPropertyOrThrow(A, Pk, mappedValue).
            A->defineOwnPropertyThrowsException(state, pk, ObjectPropertyDescriptor(mappedValue, ObjectPropertyDescriptor::AllPresent));
            // Increase k by 1.
            k++;
        }
    }
    // NOTE: items is not an Iterable so assume it is an array-like object.
    // Let arrayLike be ! ToObject(items).
    Object* arrayLike = items.toObject(state);
    // Let len be ? ToLength(? Get(arrayLike, "length")).
    auto len = arrayLike->lengthES6(state);
    // If IsConstructor(C) is true, then
    Object* A;
    if (C.isFunction() && C.asFunction()->isConstructor()) {
        // Let A be ? Construct(C, « len »).
        Value vlen(len);
        A = C.asFunction()->newInstance(state, 1, &vlen);
    } else {
        // Else,
        // Let A be ? ArrayCreate(len).
        A = new ArrayObject(state, len);
    }

    // Let k be 0.
    double k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        ObjectPropertyName Pk(state, Value(k));
        // Let kValue be ? Get(arrayLike, Pk).
        Value kValue = arrayLike->get(state, Pk).value(state, arrayLike);
        // If mapping is true, then
        Value mappedValue;
        if (mapping) {
            // Let mappedValue be ? Call(mapfn, T, « kValue, k »).
            Value argv[] = { kValue, Value(k) };
            mappedValue = mapfn.asFunction()->call(state, T, 2, argv);
        } else {
            // Else, let mappedValue be kValue.
            mappedValue = kValue;
        }
        // Perform ? CreateDataPropertyOrThrow(A, Pk, mappedValue).
        A->defineOwnPropertyThrowsException(state, Pk, ObjectPropertyDescriptor(mappedValue, ObjectPropertyDescriptor::AllPresent));
        // Increase k by 1.
        k++;
    }
    // Perform ? Set(A, "length", len, true).
    A->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().length), Value(len), A);
    // Return A.
    return A;
}

static Value builtinArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, Array, join);
    int64_t len = thisBinded->length(state);
    Value separator = argv[0];
    size_t lenMax = STRING_MAXIMUM_LENGTH;
    String* sep;

    if (separator.isUndefined()) {
        sep = state.context()->staticStrings().asciiTable[(size_t)','].string();
    } else {
        sep = separator.toString(state);
    }

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(thisBinded)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, thisBinded);

    StringBuilder builder;
    double prevIndex = 0;
    double curIndex = 0;
    while (curIndex < len) {
        if (curIndex != 0 && sep->length() > 0) {
            if (static_cast<double>(builder.contentLength()) > static_cast<double>(STRING_MAXIMUM_LENGTH - (curIndex - prevIndex - 1) * sep->length())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
            }
            while (curIndex - prevIndex > 1) {
                builder.appendString(sep);
                prevIndex++;
            }
            builder.appendString(sep);
        }
        Value elem = thisBinded->getIndexedProperty(state, Value(curIndex)).value(state, thisBinded);

        if (!elem.isUndefinedOrNull()) {
            builder.appendString(elem.toString(state));
        }
        prevIndex = curIndex;
        if (elem.isUndefined()) {
            double result;
            Object::nextIndexForward(state, thisBinded, prevIndex, len, true, result);
            curIndex = result;
        } else {
            curIndex++;
        }
    }
    if (sep->length() > 0) {
        if (static_cast<double>(builder.contentLength()) > static_cast<double>(STRING_MAXIMUM_LENGTH - (curIndex - prevIndex - 1) * sep->length())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_String_InvalidStringLength);
        }
        while (curIndex - prevIndex > 1) {
            builder.appendString(sep);
            prevIndex++;
        }
    }
    return builder.finalize(&state);
}

static Value builtinArrayReverse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, reverse);
    unsigned len = O->length(state);
    unsigned middle = std::floor(len / 2);
    unsigned lower = 0;
    while (middle > lower) {
        unsigned upper = len - lower - 1;
        ObjectPropertyName upperP = ObjectPropertyName(state, Value(upper));
        ObjectPropertyName lowerP = ObjectPropertyName(state, Value(lower));

        bool lowerExists = O->hasProperty(state, lowerP);
        Value lowerValue;
        if (lowerExists) {
            lowerValue = O->get(state, lowerP).value(state, O);
        }
        bool upperExists = O->hasProperty(state, upperP);
        Value upperValue;
        if (upperExists) {
            upperValue = O->get(state, upperP).value(state, O);
        }
        if (lowerExists && upperExists) {
            O->setThrowsException(state, lowerP, upperValue, O);
            O->setThrowsException(state, upperP, lowerValue, O);
        } else if (!lowerExists && upperExists) {
            O->setThrowsException(state, lowerP, upperValue, O);
            O->deleteOwnPropertyThrowsException(state, upperP);
        } else if (lowerExists && !upperExists) {
            O->deleteOwnPropertyThrowsException(state, lowerP);
            O->setThrowsException(state, upperP, lowerValue, O);
        } else {
            double result;
            Object::nextIndexForward(state, O, lower, middle, false, result);
            unsigned nextLower = result;
            Object::nextIndexBackward(state, O, upper, middle, false, result);
            unsigned nextUpper = result;
            unsigned x = middle - nextLower;
            unsigned y = nextUpper - middle;
            unsigned lowerCandidate;
            if (x > y) {
                lowerCandidate = nextLower;
            } else {
                lowerCandidate = len - nextUpper - 1;
            }
            if (lower == lowerCandidate)
                break;
            lower = lowerCandidate;
            continue;
        }
        lower++;
    }

    return O;
}

static Value builtinArraySort(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, sort);
    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), errorMessage_GlobalObject_FirstArgumentNotCallable);
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    thisObject->sort(state, [defaultSort, &cmpfn, &state](const Value& a, const Value& b) -> bool {
        if (a.isEmpty() && b.isUndefined())
            return false;
        if (a.isUndefined() && b.isEmpty())
            return true;
        if (a.isEmpty() || a.isUndefined())
            return false;
        if (b.isEmpty() || b.isUndefined())
            return true;
        Value arg[2] = { a, b };
        if (defaultSort) {
            String* vala = a.toString(state);
            String* valb = b.toString(state);
            return *vala < *valb;
        } else {
            Value ret = FunctionObject::call(state, cmpfn, Value(), 2, arg);
            return (ret.toNumber(state) < 0);
        } });
    return thisObject;
}

static Value builtinArraySplice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // TODO(ES6): the number of actual arguments is used.
    // e.g. var arr = [1, 2, 3, 4, 5];
    //      Different: arr.splice(2) vs. arr.splice(2, undefined)

    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, splice);

    // Let A be a new array created as if by the expression new Array()where Array is the standard built-in constructor with that name.
    ArrayObject* A = new ArrayObject(state);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToUint32(lenVal).
    int64_t len = O->length(state);

    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[0].toInteger(state);

    // If relativeStart is negative, let actualStart be max((len + relativeStart),0); else let actualStart be min(relativeStart, len).
    int64_t actualStart = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);

    // Let actualDeleteCount be min(max(ToInteger(deleteCount),0), len – actualStart).
    // below code does not follow ECMA-262, but SpiderMonkey, v8 and JSC to this..
    // int64_t actualDeleteCount = std::min(std::max(argv[1].toInteger(state), 0.0), (double)len - actualStart); // org code
    int64_t actualDeleteCount;
    double deleteCount = argv[1].toInteger(state);
    if (argc != 1) {
        actualDeleteCount = std::min(std::max(deleteCount, 0.0), (double)len - actualStart);
    } else {
        actualDeleteCount = len - actualStart;
    }

    // Let k be 0.
    int64_t k = 0;

    // Repeat, while k < actualDeleteCount
    while (k < actualDeleteCount) {
        // Let from be ToString(actualStart+k).
        ObjectPropertyName from(state, Value(actualStart + k));
        // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        // If fromPresent is true, then
        // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
        ObjectGetResult fromValue = O->getIndexedProperty(state, Value(actualStart + k));
        if (fromValue.hasValue()) {
            // Call the [[DefineOwnProperty]] internal method of A with arguments ToString(k), Property Descriptor {[[Value]]: fromValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            A->ArrayObject::defineOwnProperty(state, ObjectPropertyName(state, Value(k)),
                                              ObjectPropertyDescriptor(fromValue.value(state, O), ObjectPropertyDescriptor::AllPresent));
            // Increment k by 1.
            k++;
        } else {
            double result;
            bool exist = Object::nextIndexForward(state, O, actualStart + k, len, false, result);
            if (!exist) {
                A->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(actualDeleteCount), A);
                break;
            } else {
                k = result - actualStart;
                A->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(k), A);
            }
        }
    }

    // Let items be an internal List whose elements are, in left to right order, the portion of the actual argument list starting with item1. The list will be empty if no such items are present.
    Value* items = nullptr;
    int64_t itemCount = 0;

    if (argc > 2) {
        items = argv + 2;
        itemCount = argc - 2;
    }

    // If itemCount < actualDeleteCount, then
    if (itemCount < actualDeleteCount) {
        // Let k be actualStart.
        k = actualStart;
        // move [actualStart + deleteCnt, len) to [actualStart + insertCnt, len - deleteCnt + insertCnt)
        while (k < len - actualDeleteCount) {
            // Let from be ToString(k+actualDeleteCount).
            uint32_t from = k + actualDeleteCount;
            // Let to be ToString(k+itemCount).
            uint32_t to = k + itemCount;
            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectGetResult fromValue = O->getIndexedProperty(state, Value(from));
            // If fromPresent is true, then
            if (fromValue.hasValue()) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setIndexedPropertyThrowsException(state, Value(to), fromValue.value(state, O));
            } else {
                // Else, fromPresent is false

                // Call the [[Delete]] internal method of O with arguments to and true.
                O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(to)));
            }
            k++;
        }
        // delete [len - deleteCnt + itemCount, len)
        // Let k be len.
        k = len;
        // Repeat, while k > (len – actualDeleteCount + itemCount)
        while (k > len - actualDeleteCount + itemCount) {
            // Call the [[Delete]] internal method of O with arguments ToString(k–1) and true.
            O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(k - 1)));
            // Decrease k by 1.
            k--;
        }
    } else if (itemCount > actualDeleteCount) {
        // Else if itemCount > actualDeleteCount, then

        // Let k be (len – actualDeleteCount).
        k = len - actualDeleteCount;

        // Repeat, while k > actualStart
        while (k > actualStart) {
            // Let from be ToString(k + actualDeleteCount – 1).
            ObjectPropertyName from(state, Value(k + actualDeleteCount - 1));
            // Let to be ToString(k + itemCount – 1)
            ObjectPropertyName to(state, Value(k + itemCount - 1));

            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectGetResult fromValue = O->getIndexedProperty(state, Value(k + actualDeleteCount - 1));
            // If fromPresent is true, then
            if (fromValue.hasValue()) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setIndexedPropertyThrowsException(state, Value(k + itemCount - 1), fromValue.value(state, O));
            } else {
                // Else, fromPresent is false
                // Call the [[Delete]] internal method of O with argument to and true.
                O->deleteOwnPropertyThrowsException(state, to);
            }
            // Decrease k by 1.
            k--;
        }
    }

    // Let k be actualStart.
    k = actualStart;

    // while items is not empty
    int64_t itemsIndex = 0;
    while (itemsIndex < itemCount) {
        // Remove the first element from items and let E be the value of that element.
        Value E = items[itemsIndex++];
        // Call the [[Put]] internal method of O with arguments ToString(k), E, and true.
        O->setIndexedPropertyThrowsException(state, Value(k), E);
        // Increase k by 1.
        k++;
    }

    O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(len - actualDeleteCount + itemCount), O);
    return A;
}


static Value builtinArrayToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, toString);
    Value toString = thisObject->get(state, state.context()->staticStrings().join).value(state, thisObject);
    if (!toString.isFunction()) {
        toString = state.context()->globalObject()->objectPrototypeToString();
    }
    return FunctionObject::call(state, toString, thisObject, 0, nullptr);
}

static Value builtinArrayConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, concat);
    ArrayObject* array = new ArrayObject(state);
    uint64_t n = 0;
    for (size_t i = 0; i < argc + 1; i++) {
        Value argi = (i == 0) ? thisObject : argv[i - 1];
        if (argi.isObject()) {
            Object* arr = argi.asObject();

            // Let spreadable be IsConcatSpreadable(E).
            bool spreadable = arr->isConcatSpreadable(state);

            if (spreadable) {
                // Let k be 0.
                uint64_t k = 0;
                // Let len be the result of calling the [[Get]] internal method of E with argument "length".
                uint64_t len = arr->length(state);

                // Repeat, while k < len
                while (k < len) {
                    // Let exists be the result of calling the [[HasProperty]] internal method of E with P.
                    ObjectGetResult exists = arr->getIndexedProperty(state, Value(k));
                    if (exists.hasValue()) {
                        array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n + k)), ObjectPropertyDescriptor(exists.value(state, arr), ObjectPropertyDescriptor::AllPresent));
                        k++;
                    } else {
                        double result;
                        Object::nextIndexForward(state, arr, k, len, false, result);
                        k = result;
                    }
                }

                n += len;
                array->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(n), array);
            } else {
                array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(arr, ObjectPropertyDescriptor::AllPresent));
                n++;
            }
        } else {
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n++)), ObjectPropertyDescriptor(argi, ObjectPropertyDescriptor::AllPresent));
        }
    }

    return array;
}

static Value builtinArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, slice);
    int64_t len = thisObject->length(state);
    double relativeStart = argv[0].toInteger(state);
    int64_t k = (relativeStart < 0) ? std::max((double)len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    int64_t kStart = k;
    double relativeEnd = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    uint32_t finalEnd = (relativeEnd < 0) ? std::max((double)len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);

    int64_t n = 0;
    ArrayObject* array = new ArrayObject(state);
    while (k < finalEnd) {
        ObjectGetResult exists = thisObject->get(state, ObjectPropertyName(state, Value(k)));
        if (exists.hasValue()) {
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n)),
                                                    ObjectPropertyDescriptor(exists.value(state, thisObject), ObjectPropertyDescriptor::AllPresent));
            k++;
            n++;
        } else {
            double tmp;
            bool exist = Object::nextIndexForward(state, thisObject, k, len, false, tmp);
            if (!exist) {
                n = finalEnd - kStart;
                break;
            }
            n += tmp - k;
            k = tmp;
        }
    }
    array->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(n), array);
    return array;
}

static Value builtinArrayForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, forEach);
    uint32_t len = thisObject->length(state);

    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().forEach.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    uint32_t k = 0;
    while (k < len) {
        Value Pk = Value(k);
        auto res = thisObject->get(state, ObjectPropertyName(state, Pk));
        if (res.hasValue()) {
            Value kValue = res.value(state, thisObject);
            Value args[3] = { kValue, Pk, thisObject };
            callbackfn.asFunction()->call(state, T, 3, args);
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, thisObject, k, len, false, result);
            k = result;
            continue;
        }
    }
    return Value();
}

static Value builtinArrayIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, indexOf);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    int64_t len = O->length(state);

    // If len is 0, return -1.
    if (len == 0) {
        return Value(-1);
    }

    // If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be 0.
    double n = 0;
    if (argc > 1) {
        n = argv[1].toInteger(state);
    }

    // If n ≥ len, return -1.
    if (n >= len) {
        return Value(-1);
    }

    double k;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        k = (n == -0) ? 0 : n;
    } else {
        // Else, n<0
        // Let k be len - abs(n).
        k = len - std::abs(n);

        // If k is less than 0, then let k be 0.
        if (k < 0) {
            k = 0;
        }
    }

    // Repeat, while k<len
    while (k < len) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        ObjectGetResult kPresent = O->getIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
            Value elementK = kPresent.value(state, O);

            // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
            if (elementK.equalsTo(state, argv[0])) {
                // If same is true, return k.
                return Value(k);
            }
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
            continue;
        }
        // Increase k by 1.
        k++;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, lastIndexOf);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    int64_t len = O->length(state);

    // If len is 0, return -1.
    if (len == 0) {
        return Value(-1);
    }

    // If argument fromIndex was passed let n be ToInteger(fromIndex); else let n be len-1.
    double n;
    if (argc > 1) {
        n = argv[1].toInteger(state);
    } else {
        n = len - 1;
    }

    // If n ≥ 0, then let k be min(n, len – 1).
    double k;
    if (n >= 0) {
        k = (n == -0) ? 0 : std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len - abs(n).
        k = len - std::abs(n);
    }

    // Repeat, while k≥ 0
    while (k >= 0) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        ObjectGetResult kPresent = O->getIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
            Value elementK = kPresent.value(state, O);

            // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
            if (elementK.equalsTo(state, argv[0])) {
                // If same is true, return k.
                return Value(k);
            }
        } else {
            double result;
            Object::nextIndexBackward(state, O, k, -1, false, result);
            k = result;
            continue;
        }
        // Decrease k by 1.
        k--;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinArrayEvery(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, every);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    uint32_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let k be 0.
    uint32_t k = 0;

    while (k < len) {
        // Let Pk be ToString(k).
        Value pk(k);

        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        bool kPresent = O->hasProperty(state, ObjectPropertyName(state, pk));

        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = O->get(state, ObjectPropertyName(state, pk)).value(state, O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value args[] = { kValue, Value(k), O };
            Value testResult = callbackfn.asFunction()->call(state, T, 3, args);

            if (!testResult.toBoolean(state)) {
                return Value(false);
            }

            // Increae k by 1.
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
        }
    }
    return Value(true);
}

static Value builtinArrayFill(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, fill);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    double len = O->length(state);

    // Let relativeStart be ToInteger(start).
    double relativeStart = 0;
    if (argc > 1) {
        relativeStart = argv[1].toInteger(state);
    }

    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    unsigned k = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, len);

    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = len;
    if (argc > 2 && !argv[2].isUndefined()) {
        relativeEnd = argv[2].toInteger(state);
    }

    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    unsigned fin = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, len);

    Value value = argv[0];
    while (k < fin) {
        O->setIndexedPropertyThrowsException(state, Value(k), value);
        k++;
    }
    // return O.
    return O;
}

static Value builtinArrayFilter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, filter);

    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    uint32_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let A be a new array created as if by the expression new Array() where Array is the standard built-in constructor with that name.
    ArrayObject* A = new ArrayObject(state);

    // Let k be 0.
    uint64_t k = 0;
    // Let to be 0.
    uint64_t to = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectPropertyName Pk(state, Value(k));
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectGetResult kPresent = O->get(state, Pk);
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, O);

            // Let selected be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value v[] = { kValue, Value(k), O };
            Value selected = callbackfn.asFunction()->call(state, T, 3, v);

            // If ToBoolean(selected) is true, then
            if (selected.toBoolean(state)) {
                // Call the [[DefineOwnProperty]] internal method of A with arguments ToString(to), Property Descriptor {[[Value]]: kValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
                A->defineOwnProperty(state, ObjectPropertyName(state, Value(to)), ObjectPropertyDescriptor(kValue, ObjectPropertyDescriptor::AllPresent));
                // Increase to by 1
                to++;
            }

            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
        }
        // Increase k by 1.
    }

    // Return A.
    return A;
}

static Value builtinArrayMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, map);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    uint64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let A be a new array created as if by the expression new Array(len) where Array is the standard built-in constructor with that name and len is the value of len.
    ArrayObject* A = new ArrayObject(state);

    // Let k be 0.
    uint64_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectPropertyName Pk(state, Value(k));
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectGetResult kPresent = O->get(state, Pk);
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, O);
            // Let mappedValue be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value v[] = { kValue, Value(k), O };
            Value mappedValue = callbackfn.asFunction()->call(state, T, 3, v);
            // Call the [[DefineOwnProperty]] internal method of A with arguments Pk, Property Descriptor {[[Value]]: mappedValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            A->defineOwnProperty(state, Pk, ObjectPropertyDescriptor(mappedValue, ObjectPropertyDescriptor::AllPresent));
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
        }
        // Increase k by 1.
    }

    A->setThrowsException(state, state.context()->staticStrings().length, Value(len), A);

    return A;
}

static Value builtinArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, some);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    uint64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().some.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    uint64_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        ObjectPropertyName Pk(state, Value(k));

        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectGetResult kPresent = O->get(state, Pk);
        // If kPresent is true, then
        if (kPresent.hasValue()) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value argv[] = { kValue, Value(k), O };
            Value testResult = callbackfn.asFunction()->call(state, T, 3, argv);
            // If ToBoolean(testResult) is true, return true.
            if (testResult.toBoolean(state)) {
                return Value(true);
            }
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
            continue;
        }
        // Increase k by 1.
        k++;
    }
    // Return false.
    return Value(false);
}

// Array.prototype.includes ( searchElement [ , fromIndex ] )
static Value builtinArrayIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, includes);
    // Let len be ? ToLength(? Get(O, "length")).
    auto len = O->lengthES6(state);

    // If len is 0, return false.
    if (len == 0) {
        return Value(false);
    }

    Value searchElement = argv[0];
    // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step produces the value 0.)
    double n = argc >= 2 ? argv[1].toInteger(state) : 0;
    double k;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        k = n;
    } else {
        // Else n < 0,
        // Let k be len + n.
        k = len + n;
    }

    // If k < 0, let k be 0.
    if (k < 0) {
        k = 0;
    }

    // Repeat, while k < len
    while (k < len) {
        // Let elementK be the result of ? Get(O, ! ToString(k)).
        Value elementK = O->get(state, ObjectPropertyName(state, Value(k))).value(state, O);
        // If SameValueZero(searchElement, elementK) is true, return true.
        if (elementK.equalsToByTheSameValueZeroAlgorithm(state, searchElement)) {
            return Value(true);
        }
        // Increase k by 1.
        k++;
    }

    // Return false.
    return Value(false);
}

static Value builtinArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let array be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(array, Array, toLocaleString);

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(array)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, array);

    // Let arrayLen be the result of calling the [[Get]] internal method of array with argument "length".
    // Let len be ToUint32(arrayLen).
    int64_t len = array->length(state);

    // Let separator be the String value for the list-separator String appropriate for the host environment’s current locale (this is derived in an implementation-defined way).
    String* separator = state.context()->staticStrings().asciiTable[(size_t)','].string();

    // If len is zero, return the empty String.
    if (len == 0)
        return String::emptyString;

    // Let firstElement be the result of calling the [[Get]] internal method of array with argument "0".
    Value firstElement = array->get(state, ObjectPropertyName(state, Value(0))).value(state, array);

    // If firstElement is undefined or null, then
    Value R;
    if (firstElement.isUndefinedOrNull()) {
        // Let R be the empty String.
        R = String::emptyString;
    } else {
        // Let elementObj be ToObject(firstElement).
        Object* elementObj = firstElement.toObject(state);
        // Let func be the result of calling the [[Get]] internal method of elementObj with argument "toLocaleString".
        Value func = elementObj->get(state, state.context()->staticStrings().toLocaleString).value(state, elementObj);
        // If IsCallable(func) is false, throw a TypeError exception.
        if (!func.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);
        }
        // Let R be the result of calling the [[Call]] internal method of func providing elementObj as the this value and an empty arguments list.
        R = func.asFunction()->call(state, elementObj, 0, nullptr);
    }

    // Let k be 1.
    int64_t k = 1;

    // Repeat, while k < len
    while (k < len) {
        // Let S be a String value produced by concatenating R and separator.
        StringBuilder builder;
        builder.appendString(R.toString(state));
        builder.appendString(separator);
        String* S = builder.finalize(&state);

        // Let nextElement be the result of calling the [[Get]] internal method of array with argument ToString(k).
        Value nextElement = array->get(state, ObjectPropertyName(state, Value(k))).value(state, array);

        // If nextElement is undefined or null, then
        if (nextElement.isUndefinedOrNull()) {
            // Let R be the empty String.
            R = String::emptyString;
        } else {
            // Let elementObj be ToObject(nextElement).
            Object* elementObj = nextElement.toObject(state);
            // Let func be the result of calling the [[Get]] internal method of elementObj with argument "toLocaleString".
            Value func = elementObj->get(state, state.context()->staticStrings().toLocaleString).value(state, elementObj);
            // If IsCallable(func) is false, throw a TypeError exception.
            if (!func.isFunction()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);
            }
            // Let R be the result of calling the [[Call]] internal method of func providing elementObj as the this value and an empty arguments list.
            R = func.asFunction()->call(state, elementObj, 0, nullptr);
        }
        // Let R be a String value produced by concatenating S and R.
        StringBuilder builder2;
        builder2.appendString(S);
        builder2.appendString(R.toString(state));
        R = builder2.finalize(&state);
        // Increase k by 1.
        k++;
    }
    // Return R.
    return R;
}

static Value builtinArrayReduce(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, reduce);
    uint32_t len = O->length(state); // 2-3
    Value callbackfn = argv[0];
    Value initialValue = Value(Value::EmptyValue);
    if (argc > 1) {
        initialValue = argv[1];
    }

    if (!callbackfn.isFunction()) // 4
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), errorMessage_GlobalObject_CallbackNotCallable);

    if (len == 0 && (initialValue.isUndefined() || initialValue.isEmpty())) // 5
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), errorMessage_GlobalObject_ReduceError);

    size_t k = 0; // 6
    Value accumulator;
    if (!initialValue.isEmpty()) { // 7
        accumulator = initialValue;
    } else { // 8
        bool kPresent = false; // 8.a
        while (kPresent == false && k < len) { // 8.b
            Value Pk = Value(k); // 8.b.i
            kPresent = O->hasProperty(state, ObjectPropertyName(state, Pk)); // 8.b.ii
            if (kPresent)
                accumulator = O->get(state, ObjectPropertyName(state, Pk)).value(state, O); // 8.b.iii.1
            k++; // 8.b.iv
        }
        if (kPresent == false)
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), errorMessage_GlobalObject_ReduceError);
    }
    while (k < len) { // 9
        Value Pk = Value(k); // 9.a
        bool kPresent = O->hasProperty(state, ObjectPropertyName(state, Pk)); // 9.b
        if (kPresent) { // 9.c
            Value kValue = O->get(state, ObjectPropertyName(state, Pk)).value(state, O); // 9.c.i
            const int fnargc = 4;
            Value fnargs[] = { accumulator, kValue, Value(k), O };
            accumulator = FunctionObject::call(state, callbackfn, Value(), fnargc, fnargs);
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            k = result;
        }
    }
    return accumulator;
}

static Value builtinArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, reduceRight);

    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToUint32(lenValue).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().reduceRight.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }

    // If len is 0 and initialValue is not present, throw a TypeError exception.
    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduceRight.string(), errorMessage_GlobalObject_ReduceError);
    }

    // Let k be len-1.
    int64_t k = len - 1;

    Value accumulator;
    // If initialValue is present, then
    if (argc > 1) {
        // Set accumulator to initialValue.
        accumulator = argv[1];
    } else {
        // Else, initialValue is not present
        // Let kPresent be false.
        bool kPresent = false;

        // Repeat, while kPresent is false and k ≥ 0
        while ((kPresent == false) && k >= 0) {
            // Let Pk be ToString(k).
            ObjectPropertyName Pk(state, Value(k));
            // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
            ObjectGetResult test = O->get(state, Pk);
            kPresent = test.hasValue();

            // If kPresent is true, then
            if (kPresent) {
                // Let accumulator be the result of calling the [[Get]] internal method of O with argument Pk.
                accumulator = test.value(state, O);
            }

            // Decrease k by 1.
            double result;
            Object::nextIndexBackward(state, O, k, -1, false, result);
            k = result;
        }
        // If kPresent is false, throw a TypeError exception.
        if (kPresent == false) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduceRight.string(), errorMessage_GlobalObject_ReduceError);
        }
    }

    // Repeat, while k ≥ 0
    while (k >= 0) {
        // Let Pk be ToString(k).
        ObjectPropertyName Pk(state, Value(k));
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectGetResult test = O->get(state, Pk);
        bool kPresent = test.hasValue();
        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = test.value(state, O);

            // Let accumulator be the result of calling the [[Call]] internal method of callbackfn with undefined as the this value and argument list containing accumulator, kValue, k, and O.
            Value v[] = { accumulator, kValue, Value(k), O };
            accumulator = callbackfn.asFunction()->call(state, Value(), 4, v);
        }

        // Decrease k by 1.
        double result;
        Object::nextIndexBackward(state, O, k, -1, false, result);
        k = result;
    }

    // Return accumulator.
    return accumulator;
}

static Value builtinArrayPop(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, pop);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToUint32(lenVal).
    uint32_t len = O->length(state);

    // If len is zero,
    if (len == 0) {
        // Call the [[Put]] internal method of O with arguments "length", 0, and true.
        O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(0), O);
        // Return undefined.
        return Value();
    } else {
        // Else, len > 0
        // Let indx be ToString(len–1).
        ObjectPropertyName indx(state, Value(len - 1));
        // Let element be the result of calling the [[Get]] internal method of O with argument indx.
        Value element = O->get(state, indx).value(state, O);
        // Call the [[Delete]] internal method of O with arguments indx and true.
        O->deleteOwnPropertyThrowsException(state, indx);
        // Call the [[Put]] internal method of O with arguments "length", indx, and true.
        O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(len - 1), O);
        // Return element.
        return element;
    }
}

static Value builtinArrayPush(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Array.prototype.push ( [ item1 [ , item2 [ , … ] ] ] )
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, push);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let n be ToUint32(lenVal).
    int64_t n = O->length(state);
    // Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
    // Repeat, while items is not empty
    // Remove the first element from items and let E be the value of the element.
    for (size_t i = 0; i < argc; i++) {
        // Call the [[Put]] internal method of O with arguments ToString(n), E, and true.
        O->setIndexedPropertyThrowsException(state, Value(n), argv[i]);
        // Increase n by 1.
        n++;
    }

    // Call the [[Put]] internal method of O with arguments "length", n, and true.
    O->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, state.context()->staticStrings().length), Value(n), O);

    // Return n.
    return Value(n);
}

static Value builtinArrayShift(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, shift);
    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToUint32(lenVal).
    int64_t len = O->length(state);
    // If len is zero, then
    if (len == 0) {
        // Call the [[Put]] internal method of O with arguments "length", 0, and true.
        O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(0), O);
        // Return undefined.
        return Value();
    }
    // Let first be the result of calling the [[Get]] internal method of O with argument "0".
    Value first = O->get(state, ObjectPropertyName(state, Value(0))).value(state, O);
    // Let k be 1.
    int64_t k = 1;
    // Repeat, while k < len
    while (k < len) {
        // Let from be ToString(k).
        ObjectPropertyName from(state, Value(k));
        // Let to be ToString(k–1).
        ObjectPropertyName to(state, Value(k - 1));
        // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        ObjectGetResult fromPresent = O->get(state, from);

        // If fromPresent is true, then
        if (fromPresent.hasValue()) {
            // Let fromVal be the result of calling the [[Get]] internal method of O with argument from.
            Value fromVal = fromPresent.value(state, O);
            // Call the [[Put]] internal method of O with arguments to, fromVal, and true.
            O->setThrowsException(state, to, fromVal, O);
        } else {
            // Else, fromPresent is false
            // Call the [[Delete]] internal method of O with arguments to and true.
            O->deleteOwnPropertyThrowsException(state, to);
        }

        // Increase k by 1.
        if (fromPresent.hasValue()) {
            k++;
        } else {
            double result;
            Object::nextIndexForward(state, O, k, len, false, result);
            double r = result;
            if (r > k) {
                k = r;
            } else {
                k--;
            }
        }
    }
    // Call the [[Delete]] internal method of O with arguments ToString(len–1) and true.
    O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(len - 1)));
    // Call the [[Put]] internal method of O with arguments "length", (len–1) , and true.
    O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(len - 1), O);
    // Return first.
    return first;
}

static Value builtinArrayUnshift(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, unshift);
    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToUint32(lenVal).
    int64_t len = O->length(state);

    // Let argCount be the number of actual arguments.
    size_t argCount = argc;
    // Let k be len.
    int64_t k = len;

    // If argCount > 0, then
    // this line add in newer version ECMAScript than ECMAScript 5.1
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-array.prototype.unshift
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.4.4.13
    if (argCount) {
        // Repeat, while k > 0,
        while (k > 0) {
            // Let from be ToString(k–1).
            ObjectPropertyName from(state, Value(k - 1));
            // Let to be ToString(k+argCount –1).
            ObjectPropertyName to(state, Value(k + argCount - 1));

            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectGetResult fromPresent = O->get(state, from);
            // If fromPresent is true, then
            if (fromPresent.hasValue()) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                Value fromValue = fromPresent.value(state, O);
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setThrowsException(state, to, fromValue, O);
            } else {
                // Else, fromPresent is false
                // Call the [[Delete]] internal method of O with arguments to, and true.
                O->deleteOwnPropertyThrowsException(state, to);
            }

            if (fromPresent.hasValue()) {
                // Decrease k by 1.
                k--;
            } else {
                double result;
                Object::nextIndexBackward(state, O, k, -1, false, result);
                double r = std::max(result + 1, result - argCount + 1);
                if (r < k && std::abs(r - k) > argCount) {
                    k = r;
                } else {
                    k--;
                }
            }
        }

        // Let j be 0.
        size_t j = 0;
        // Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
        Value* items = argv;

        // Repeat, while items is not empty
        while (j < argCount) {
            // Remove the first element from items and let E be the value of that element.
            Value E = items[j];
            // Call the [[Put]] internal method of O with arguments ToString(j), E, and true.
            O->setThrowsException(state, ObjectPropertyName(state, Value(j)), E, O);
            // Increase j by 1.
            j++;
        }
    }

    // Call the [[Put]] internal method of O with arguments "length", len+argCount, and true.
    O->setThrowsException(state, state.context()->staticStrings().length, Value(len + argCount), O);

    // Return len+argCount.
    return Value(len + argCount);
}

// Array.prototype.find ( predicate [ , thisArg ] )#
static Value builtinArrayFind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, find);
    // Let len be ? ToLength(? Get(O, "length")).
    double len = O->lengthES6(state);
    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!argv[0].isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().find.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc >= 2) {
        T = argv[1];
    }
    // Let k be 0.
    double k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->get(state, ObjectPropertyName(state, Value(k))).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, T, « kValue, k, O »)).
        Value v[] = { kValue, Value(k), O };
        bool testResult = argv[0].asFunction()->call(state, T, 3, v).toBoolean(state);
        // If testResult is true, return kValue.
        if (testResult) {
            return kValue;
        }
        // Increase k by 1.
        k++;
    }
    // Return undefined.
    return Value();
}

// Array.prototype.findIndex ( predicate [ , thisArg ] )#
static Value builtinArrayFindIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let O be ? ToObject(this value).
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, findIndex);
    // Let len be ? ToLength(? Get(O, "length")).
    double len = O->lengthES6(state);
    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!argv[0].isFunction()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().findIndex.string(), errorMessage_GlobalObject_CallbackNotCallable);
    }
    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc >= 2) {
        T = argv[1];
    }
    // Let k be 0.
    double k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->get(state, ObjectPropertyName(state, Value(k))).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, T, « kValue, k, O »)).
        Value v[] = { kValue, Value(k), O };
        bool testResult = argv[0].asFunction()->call(state, T, 3, v).toBoolean(state);
        // If testResult is true, return k.
        if (testResult) {
            return Value(k);
        }
        // Increase k by 1.
        k++;
    }
    // Return -1
    return Value(-1);
}

static Value builtinArrayKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, Array, keys);
    return M->keys(state);
}

static Value builtinArrayValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, Array, values);
    return M->values(state);
}

static Value builtinArrayEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(M, Array, entries);
    return M->entries(state);
}

static Value builtinArrayIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isIteratorObject() || !thisValue.asObject()->asIteratorObject()->isArrayIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().ArrayIterator.string(), true, state.context()->staticStrings().next.string(), errorMessage_GlobalObject_CalledOnIncompatibleReceiver);
    }
    ArrayIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asArrayIteratorObject();
    return iter->next(state);
}

void GlobalObject::installArray(ExecutionState& state)
{
    m_array = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
                                     return new ArrayObject(state);
                                 }),
                                 FunctionObject::__ForBuiltin__);
    m_array->markThisObjectDontNeedStructureTransitionTable(state);
    m_array->setPrototype(state, m_functionPrototype);
    m_arrayPrototype = m_objectPrototype;
    m_arrayPrototype = new ArrayObject(state);
    m_arrayPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_arrayPrototype->setPrototype(state, m_objectPrototype);
    m_arrayPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().isArray),
                                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isArray, builtinArrayIsArray, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().from),
                                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().from, builtinArrayFrom, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinArrayConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinArrayForEach, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().indexOf),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinArrayIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().lastIndexOf),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().lastIndexOf, builtinArrayLastIndexOf, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().join),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().join, builtinArrayJoin, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sort),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sort, builtinArraySort, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().splice),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().splice, builtinArraySplice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().slice),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().slice, builtinArraySlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().every),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().every, builtinArrayEvery, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().fill),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().every, builtinArrayFill, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().includes),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().every, builtinArrayIncludes, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().filter),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().filter, builtinArrayFilter, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().reduce),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reduce, builtinArrayReduce, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().reduceRight),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reduceRight, builtinArrayReduceRight, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().pop),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().pop, builtinArrayPop, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().push),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().push, builtinArrayPush, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().shift),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().shift, builtinArrayShift, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().reverse),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reverse, builtinArrayReverse, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinArrayToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().map),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().map, builtinArrayMap, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().some),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().some, builtinArraySome, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toLocaleString),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleString, builtinArrayToLocaleString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().unshift),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().unshift, builtinArrayUnshift, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().keys),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().keys, builtinArrayKeys, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().find),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().find, builtinArrayFind, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().findIndex),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().findIndex, builtinArrayFindIndex, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    FunctionObject* values = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinArrayValues, 0, nullptr, NativeFunctionInfo::Strict));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().values),
                                                       ObjectPropertyDescriptor(values, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().iterator),
                                                       ObjectPropertyDescriptor(values,
                                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinArrayEntries, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    m_arrayIteratorPrototype = m_iteratorPrototype;
    m_arrayIteratorPrototype = new ArrayIteratorObject(state, nullptr, ArrayIteratorObject::TypeKey);

    m_arrayIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinArrayIteratorNext, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayIteratorPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                               ObjectPropertyDescriptor(Value(String::fromASCII("Array Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
