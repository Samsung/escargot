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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/ArrayObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/ToStringRecursionPreventer.h"
#include "runtime/ErrorObject.h"
#include "runtime/NativeFunctionObject.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool interpretArgumentsAsElements = false;
    uint64_t size = 0;
    if (argc > 1) {
        size = argc;
        interpretArgumentsAsElements = true;
    } else if (argc == 1) {
        Value& val = argv[0];
        if (val.isNumber()) {
            if (val.equalsTo(state, Value(val.toUint32(state)))) {
                size = val.toNumber(state);
            } else {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength);
            }
        } else {
            size = 1;
            interpretArgumentsAsElements = true;
        }
    }

    // If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    if (!newTarget.hasValue()) {
        newTarget = state.resolveCallee();
    }

    // Let proto be ? GetPrototypeFromConstructor(newTarget, "%ArrayPrototype%").
    // Let array be ! ArrayCreate(0, proto).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->arrayPrototype();
    });
    ArrayObject* array = new ArrayObject(state, proto, size);

    if (interpretArgumentsAsElements) {
        if (argc > 1 || !argv[0].isNumber()) {
            if (array->isFastModeArray()) {
                for (size_t idx = 0; idx < argc; idx++) {
                    array->m_fastModeData[idx] = argv[idx];
                }
            } else {
                Value val = argv[0];
                for (size_t idx = 0; idx < argc; idx++) {
                    array->setIndexedProperty(state, Value(idx), argv[idx], array);
                }
            }
        }
    }
    return array;
}

#define CHECK_ARRAY_LENGTH(COND)                                                                                             \
    if (COND) {                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_InvalidArrayLength); \
    }

static Object* arraySpeciesCreate(ExecutionState& state, Object* originalArray, const int64_t length)
{
    ASSERT(originalArray != nullptr);
    // Assert: length is an integer Number >= 0.
    ASSERT(length >= 0);

    // Let C be undefined.
    Value C;
    // Let isArray be IsArray(originalArray).
    // If isArray is true, then
    if (originalArray->isArray(state)) {
        // Let C be Get(originalArray, "constructor").
        C = originalArray->get(state, ObjectPropertyName(state.context()->staticStrings().constructor)).value(state, originalArray);

        // If IsConstructor(C) is true, then
        if (C.isConstructor()) {
            // Let thisRealm be the running execution context’s Realm.
            Context* thisRealm = state.context();
            // Let realmC be GetFunctionRealm(C).
            Context* realmC = C.asObject()->getFunctionRealm(state);

            // ReturnIfAbrupt(realmC).
            // If thisRealm and realmC are not the same Realm Record, then
            // If SameValue(C, realmC.[[intrinsics]].[[%Array%]]) is true, let C be undefined.
            if (thisRealm != realmC) {
                if (C.asPointerValue() == realmC->globalObject()->array()) {
                    C = Value();
                }
            }
        }
        // If Type(C) is Object, then
        if (C.isObject()) {
            // a. Set C be Get(C, @@species).
            C = C.asObject()->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species)).value(state, C);
            if (C.isNull()) { // b. If C is null, set C to undefined.
                C = Value();
            }
        }
    }

    // If C is undefined, return ArrayCreate(length).
    if (C.isUndefined()) {
        return new ArrayObject(state, static_cast<uint64_t>(length));
    }
    // If IsConstructor(C) is false, throw a TypeError exception.
    if (!C.isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_ThisNotConstructor);
    }
    // Return Construct(C, <<length>>).
    Value argv[1] = { Value(length) };
    return Object::construct(state, C, 1, argv).toObject(state);
}

// http://ecma-international.org/ecma-262/10.0/#sec-flattenintoarray
// FlattenIntoArray(target, source, sourceLen, start, depth [ , mapperFunction, thisArg ])
static int64_t flattenIntoArray(ExecutionState& state, Value target, Value source, int64_t sourceLen, int64_t start, double depth, Value mappedValue = Value(Value::EmptyValue), Value thisArg = Value(Value::EmptyValue))
{
    ASSERT(target.isObject());
    ASSERT(source.isObject());
    ASSERT(sourceLen >= 0);

    int64_t targetIndex = start;
    int64_t sourceIndex = 0;

    while (sourceIndex < sourceLen) {
        String* p = Value(sourceIndex).toString(state);
        ObjectHasPropertyResult exists = source.asObject()->hasIndexedProperty(state, p);
        if (exists) {
            Value element = exists.value(state, ObjectPropertyName(state, p), source);
            if (!mappedValue.isEmpty()) {
                Value args[] = { element, Value(sourceIndex), source };
                ASSERT(!thisArg.isEmpty() && depth == 1);
                element = Object::call(state, mappedValue, thisArg, 3, args);
            }
            if (depth > 0 && element.isObject() && element.asObject()->isArray(state)) {
                int64_t elementLen = element.asObject()->length(state);
                targetIndex = flattenIntoArray(state, target, element, elementLen, targetIndex, depth - 1);

            } else {
                if (targetIndex >= std::numeric_limits<int64_t>::max()) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "invalid index");
                }
                target.asObject()->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, targetIndex),
                                                                    ObjectPropertyDescriptor(element, ObjectPropertyDescriptor::AllPresent));
                targetIndex++;
            }
        }
        sourceIndex++;
    }
    return targetIndex;
}

static Value builtinArrayIsArray(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ASSERT(argv != nullptr);

    return Value(argv[0].isObject() && argv[0].asObject()->isArray(state));
}

// Array.from ( items [ , mapfn [ , thisArg ] ] )#
static Value builtinArrayFrom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
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
        if (!mapfn.isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "argument map function should be undefined or function");
        }
        // If thisArg was supplied, let T be thisArg; else let T be undefined.
        T = thisArg;
        // Let mapping be true.
        mapping = true;
    }

    // Let usingIterator be ? GetMethod(items, @@iterator).
    Value usingIterator = Object::getMethod(state, items, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator));
    // If usingIterator is not undefined, then
    if (!usingIterator.isUndefined()) {
        Object* A;
        // If IsConstructor(C) is true, then
        if (C.isConstructor()) {
            // Let A be ? Construct(C).
            A = Object::construct(state, C, 0, nullptr).toObject(state);
        } else {
            // Let A be ArrayCreate(0).
            A = new ArrayObject(state);
        }
        // Let iteratorRecord be ? GetIterator(items, sync, usingIterator).
        IteratorRecord* iteratorRecord = IteratorObject::getIterator(state, items, true, usingIterator);

        // Let k be 0.
        int64_t k = 0;
        // Repeat
        while (true) {
            // If k ≥ 2^53-1, then
            if (k >= ((1LL << 53LL) - 1LL)) {
                // Let error be ThrowCompletion(a newly created TypeError object).
                // Return ? IteratorClose(iteratorRecord, error).
                Value throwCompletion = ErrorObject::createError(state, ErrorCode::TypeError, new ASCIIString("Got invalid index"));
                return IteratorObject::iteratorClose(state, iteratorRecord, throwCompletion, true);
            }
            // Let Pk be ! ToString(k).
            ObjectPropertyName pk(state, k);
            // Let next be ? IteratorStep(iteratorRecord).
            Optional<Object*> next = IteratorObject::iteratorStep(state, iteratorRecord);
            // If next is false, then
            if (!next.hasValue()) {
                // Perform ? Set(A, "length", k, true).
                A->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().length), Value(k), A);
                // Return A.
                return A;
            }
            // Let nextValue be ? IteratorValue(next).
            Value nextValue = IteratorObject::iteratorValue(state, next.value());
            Value mappedValue;
            // If mapping is true, then
            if (mapping) {
                // Let mappedValue be Call(mapfn, T, « nextValue, k »).
                // If mappedValue is an abrupt completion, return ? IteratorClose(iteratorRecord, mappedValue).
                // Set mappedValue to mappedValue.[[Value]].
                Value argv[] = { nextValue, Value(k) };
                try {
                    mappedValue = Object::call(state, mapfn, T, 2, argv);
                } catch (const Value& v) {
                    Value exceptionValue = v;
                    return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
                }
            } else {
                mappedValue = nextValue;
            }

            try {
                // Let defineStatus be CreateDataPropertyOrThrow(A, Pk, mappedValue).
                A->defineOwnPropertyThrowsException(state, pk, ObjectPropertyDescriptor(mappedValue, ObjectPropertyDescriptor::AllPresent));
            } catch (const Value& v) {
                // If defineStatus is an abrupt completion, return ? IteratorClose(iteratorRecord, defineStatus).
                Value exceptionValue = v;
                return IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
            }
            // Increase k by 1.
            k++;
        }
    }
    // NOTE: items is not an Iterable so assume it is an array-like object.
    // Let arrayLike be ! ToObject(items).
    Object* arrayLike = items.toObject(state);
    // Let len be ? ToLength(? Get(arrayLike, "length")).
    uint64_t len = arrayLike->length(state);
    // If IsConstructor(C) is true, then
    Object* A;
    if (C.isConstructor()) {
        // Let A be ? Construct(C, « len »).
        Value vlen(len);
        A = Object::construct(state, C, 1, &vlen).toObject(state);
    } else {
        // Else,
        // Let A be ? ArrayCreate(len).
        A = new ArrayObject(state, len);
    }

    // Let k be 0.
    uint64_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        ObjectPropertyName Pk(state, k);
        // Let kValue be ? Get(arrayLike, Pk).
        Value kValue = arrayLike->getIndexedPropertyValue(state, Value(k), arrayLike);
        // If mapping is true, then
        Value mappedValue;
        if (mapping) {
            // Let mappedValue be ? Call(mapfn, T, « kValue, k »).
            Value argv[] = { kValue, Value(k) };
            mappedValue = Object::call(state, mapfn, T, 2, argv);
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

// Array.of ( ...items )
static Value builtinArrayOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    size_t len = argc;
    Value C = thisValue;

    Object* A;
    if (C.isConstructor()) {
        Value arg[1] = { Value(len) };
        A = Object::construct(state, C, 1, arg).toObject(state);
    } else {
        A = new ArrayObject(state, static_cast<uint64_t>(len));
    }

    size_t k = 0;
    while (k < len) {
        Value kValue = argv[k];
        ObjectPropertyName Pk(state, k);
        A->defineOwnPropertyThrowsException(state, Pk, ObjectPropertyDescriptor(kValue, ObjectPropertyDescriptor::AllPresent));
        k++;
    }
    A->setThrowsException(state, ObjectPropertyName(state, state.context()->staticStrings().length), Value(len), A);

    return A;
}

static Value builtinArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisBinded = thisValue.toObject(state);
    int64_t len = thisBinded->length(state);
    Value separator = argv[0];
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
    int64_t prevIndex = 0;
    int64_t curIndex = 0;
    while (curIndex < len) {
        if (curIndex != 0 && sep->length() > 0) {
            if (static_cast<double>(builder.contentLength()) > static_cast<double>(STRING_MAXIMUM_LENGTH - (curIndex - prevIndex - 1) * (int64_t)sep->length())) {
                ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
            }
            while (curIndex - prevIndex > 1) {
                builder.appendString(sep);
                prevIndex++;
            }
            builder.appendString(sep);
        }
        Value elem = thisBinded->getIndexedPropertyValue(state, Value(curIndex), thisBinded);

        if (!elem.isUndefinedOrNull()) {
            builder.appendString(elem.toString(state));
        }
        prevIndex = curIndex;
        if (elem.isUndefined()) {
            struct Data {
                bool exists;
                int64_t cur;
                int64_t ret;
            } data;
            data.exists = false;
            data.cur = curIndex;
            data.ret = len;

            Value ptr = thisBinded;
            while (ptr.isObject()) {
                if (!ptr.asObject()->isOrdinary()) {
                    curIndex++;
                    break;
                }
                ptr.asObject()->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) {
                    int64_t index;
                    Data* e = (Data*)data;
                    int64_t* ret = &e->ret;
                    Value key = name.toPlainValue();
                    index = key.toNumber(state);
                    if ((uint64_t)index != Value::InvalidIndexValue) {
                        if (self->get(state, name).value(state, self).isUndefined()) {
                            return true;
                        }
                        if (index > e->cur && e->ret > index) {
                            e->ret = std::min(index, e->ret);
                        }
                    }
                    return true;
                },
                                            &data);
                ptr = ptr.asObject()->getPrototype(state);
            }
            curIndex = data.ret;
        } else {
            curIndex++;
        }
    }
    if (sep->length() > 0) {
        if (static_cast<double>(builder.contentLength()) > static_cast<double>(STRING_MAXIMUM_LENGTH - (curIndex - prevIndex - 1) * (int64_t)sep->length())) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
        }
        while (curIndex - prevIndex > 1) {
            builder.appendString(sep);
            prevIndex++;
        }
    }
    return builder.finalize(&state);
}

static Value builtinArrayReverse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    int64_t len = O->length(state);
    int64_t middle = std::floor(len / 2);
    int64_t lower = 0;
    while (middle > lower) {
        int64_t upper = len - lower - 1;
        ObjectPropertyName upperP = ObjectPropertyName(state, upper);
        ObjectPropertyName lowerP = ObjectPropertyName(state, lower);

        auto lowerExists = O->hasIndexedProperty(state, Value(lower));
        Value lowerValue;
        if (lowerExists) {
            lowerValue = lowerExists.value(state, lowerP, O);
        }
        auto upperExists = O->hasIndexedProperty(state, Value(upper));
        Value upperValue;
        if (upperExists) {
            upperValue = upperExists.value(state, upperP, O);
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
            int64_t result;
            Object::nextIndexForward(state, O, lower, middle, result);
            int64_t nextLower = result;
            Object::nextIndexBackward(state, O, upper, middle, result);
            int64_t nextUpper = result;
            int64_t x = middle - nextLower;
            int64_t y = nextUpper - middle;
            int64_t lowerCandidate;
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

static Value builtinArraySort(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    Object* thisObject = thisValue.toObject(state);
    uint64_t len = thisObject->length(state);

    thisObject->sort(state, len, [defaultSort, &cmpfn, &state](const Value& a, const Value& b) -> bool {
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
            Value ret = Object::call(state, cmpfn, Value(), 2, arg);
            return (ret.toNumber(state) < 0);
        } });
    return thisObject;
}

static Value builtinArraySplice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // TODO(ES6): the number of actual arguments is used.
    // e.g. var arr = [1, 2, 3, 4, 5];
    //      Different: arr.splice(2) vs. arr.splice(2, undefined)

    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[0].toInteger(state);

    // If relativeStart is negative, let actualStart be max((len + relativeStart),0); else let actualStart be min(relativeStart, len).
    int64_t actualStart = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);

    int64_t insertCount = 0;
    int64_t actualDeleteCount = 0;

    if (argc == 1) {
        // Else if deleteCount is not present, then
        // Let actualDeleteCount be len - actualStart.
        actualDeleteCount = len - actualStart;
    } else if (argc > 1) {
        // Else,
        // Let insertCount be the number of actual arguments minus 2.
        insertCount = argc - 2;
        // Let dc be ToInteger(deleteCount).
        double dc = argv[1].toInteger(state);
        // Let actualDeleteCount be min(max(dc,0), len – actualStart).
        actualDeleteCount = std::min(std::max(dc, 0.0), (double)(len - actualStart));
    }

    // If len+insertCount−actualDeleteCount > 2^53-1, throw a TypeError exception.
    CHECK_ARRAY_LENGTH(len + insertCount - actualDeleteCount > Value::maximumLength());
    // Let A be ArraySpeciesCreate(O, actualDeleteCount).
    Object* A = arraySpeciesCreate(state, O, actualDeleteCount);

    // Let k be 0.
    int64_t k = 0;

    // Repeat, while k < actualDeleteCount
    while (k < actualDeleteCount) {
        // Let from be ToString(actualStart+k).
        // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        // If fromPresent is true, then
        // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
        ObjectHasPropertyResult fromValue = O->hasIndexedProperty(state, Value(actualStart + k));
        if (fromValue) {
            // Call the [[DefineOwnProperty]] internal method of A with arguments ToString(k), Property Descriptor {[[Value]]: fromValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            ObjectPropertyName from(state, Value(actualStart + k));
            A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, k),
                                                ObjectPropertyDescriptor(fromValue.value(state, from, O), ObjectPropertyDescriptor::AllPresent));
        }
        // Increment k by 1.
        k++;
    }
    // Let setStatus be Set(A, "length", actualDeleteCount, true).
    A->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(actualDeleteCount), A);

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
            int64_t from = k + actualDeleteCount;
            // Let to be ToString(k+itemCount).
            int64_t to = k + itemCount;
            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectHasPropertyResult fromValue = O->hasIndexedProperty(state, Value(from));
            // If fromPresent is true, then
            if (fromValue) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setIndexedPropertyThrowsException(state, Value(to), fromValue.value(state, ObjectPropertyName(state, from), O));
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
            // Let to be ToString(k + itemCount – 1)

            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectHasPropertyResult fromValue = O->hasIndexedProperty(state, Value(k + actualDeleteCount - 1));
            // If fromPresent is true, then
            if (fromValue) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                ObjectPropertyName from(state, k + actualDeleteCount - 1);
                O->setIndexedPropertyThrowsException(state, Value(k + itemCount - 1), fromValue.value(state, from, O));
            } else {
                // Else, fromPresent is false
                // Call the [[Delete]] internal method of O with argument to and true.
                ObjectPropertyName to(state, k + itemCount - 1);
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

static Value builtinArrayConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    Object* obj = arraySpeciesCreate(state, thisObject, 0);
    int64_t n = 0;
    for (size_t i = 0; i < argc + 1; i++) {
        Value argi = (i == 0) ? thisObject : argv[i - 1];
        if (argi.isObject()) {
            Object* arr = argi.asObject();

            // Let spreadable be IsConcatSpreadable(E).
            bool spreadable = arr->isConcatSpreadable(state);

            if (spreadable) {
                // Let k be 0.
                int64_t k = 0;
                // Let len be the result of calling the [[Get]] internal method of E with argument "length".
                int64_t len = arr->length(state);

                // If n + len > 2^53 - 1, throw a TypeError exception.
                CHECK_ARRAY_LENGTH(n + len > Value::maximumLength());

                // Repeat, while k < len
                while (k < len) {
                    // Let exists be the result of calling the [[HasProperty]] internal method of E with P.
                    ObjectHasPropertyResult exists = arr->hasIndexedProperty(state, Value(k));
                    if (exists) {
                        obj->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n + k)), ObjectPropertyDescriptor(exists.value(state, ObjectPropertyName(state, k), arr), ObjectPropertyDescriptor::AllPresent));
                        k++;
                    } else {
                        int64_t result;
                        Object::nextIndexForward(state, arr, k, len, result);
                        k = result;
                    }
                }

                n += len;
                obj->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(n), obj);
            } else {
                // If n >= 2^53 - 1, throw a TypeError exception.
                CHECK_ARRAY_LENGTH(n >= Value::maximumLength());

                obj->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n)), ObjectPropertyDescriptor(arr, ObjectPropertyDescriptor::AllPresent));
                n++;
            }
        } else {
            obj->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n++)), ObjectPropertyDescriptor(argi, ObjectPropertyDescriptor::AllPresent));
        }
    }

    return obj;
}

static Value builtinArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    int64_t len = thisObject->length(state);
    double relativeStart = argv[0].toInteger(state);
    int64_t k = (relativeStart < 0) ? std::max((double)len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    int64_t kStart = k;
    double relativeEnd = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    int64_t finalEnd = (relativeEnd < 0) ? std::max((double)len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);

    int64_t n = 0;
    // Let count be max(final - k, 0).
    // Let A be ArraySpeciesCreate(O, count).
    Object* ArrayObject = arraySpeciesCreate(state, thisObject, std::max(((int64_t)finalEnd - (int64_t)k), (int64_t)0));
    while (k < finalEnd) {
        ObjectHasPropertyResult exists = thisObject->hasIndexedProperty(state, Value(k));
        if (exists) {
            ArrayObject->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n)),
                                                          ObjectPropertyDescriptor(exists.value(state, ObjectPropertyName(state, k), thisObject), ObjectPropertyDescriptor::AllPresent));
            k++;
            n++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, thisObject, k, len, result);
            n += result - k;
            k = result;
        }
    }
    if (finalEnd - kStart > 0) {
        ArrayObject->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(finalEnd - kStart), Value(ArrayObject));
    } else {
        ArrayObject->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(0), Value(ArrayObject));
    }
    return ArrayObject;
}

static Value builtinArrayToSorted(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value cmpfn = argv[0];
    if (!cmpfn.isUndefined() && !cmpfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toSorted.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    Object* thisObject = thisValue.toObject(state);
    uint64_t len = thisObject->length(state);

    ArrayObject* arr = new ArrayObject(state, len);
    thisObject->toSorted(state, arr, len, [defaultSort, &cmpfn, &state](const Value& a, const Value& b) -> bool {
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
            Value ret = Object::call(state, cmpfn, Value(), 2, arg);
            return (ret.toNumber(state) < 0);
        } });

    return arr;
}

static Value builtinArrayToSpliced(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);

    int64_t len = O->length(state);

    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[0].toInteger(state);

    // If relativeStart is negative, let actualStart be max((len + relativeStart),0); else let actualStart be min(relativeStart, len).
    int64_t actualStart = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);

    int64_t insertCount = 0;
    int64_t actualSkipCount = 0;

    if (argc == 1) {
        // Else if deleteCount is not present, then
        // Let actualSkipCount be len - actualStart.
        actualSkipCount = len - actualStart;
    } else if (argc > 1) {
        // Else,
        // Let insertCount be the number of actual arguments minus 2.
        insertCount = argc - 2;
        // Let dc be ToInteger(deleteCount).
        double dc = argv[1].toInteger(state);
        // Let actualSkipCount be min(max(dc,0), len – actualStart).
        actualSkipCount = std::min(std::max(dc, 0.0), (double)(len - actualStart));
    }

    // Let newLen be len + insertCount - actualSkipCount.
    int64_t newLen = len + insertCount - actualSkipCount;
    CHECK_ARRAY_LENGTH(newLen > Value::maximumLength());

    // Let A be ? ArrayCreate(newLen).
    ArrayObject* A = new ArrayObject(state, static_cast<uint64_t>(newLen));

    // Let i be 0.
    int64_t i = 0;
    // Let r be actualStart + actualSkipCount.
    int64_t r = actualStart + actualSkipCount;

    // Repeat, while i < actualStart,
    while (i < actualStart) {
        // Perform ! CreateDataPropertyOrThrow(A, Pi, iValue).
        A->setIndexedProperty(state, Value(i), O->getIndexedPropertyValue(state, Value(i), O), A);
        i++;
    }

    // For each element E of items, do
    for (int64_t k = 0; k < insertCount; k++) {
        A->setIndexedProperty(state, Value(i), argv[k + 2], A);
        i++;
    }

    // Repeat, while i < newLen,
    while (i < newLen) {
        Value fromValue = O->getIndexedPropertyValue(state, Value(r), O);
        A->setIndexedProperty(state, Value(i), fromValue, A);
        i++;
        r++;
    }

    return A;
}

static Value builtinArrayToReversed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    uint64_t len = O->length(state);

    ArrayObject* A = new ArrayObject(state, len);
    uint64_t k = 0;

    while (k < len) {
        Value fromValue = O->getIndexedPropertyValue(state, Value(len - k - 1), O);
        A->setIndexedProperty(state, Value(k), fromValue, A);
        k++;
    }

    return A;
}

static Value builtinArrayForEach(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    int64_t len = thisObject->length(state);

    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().forEach.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    int64_t k = 0;
    while (k < len) {
        Value Pk = Value(k);
        auto res = thisObject->hasProperty(state, ObjectPropertyName(state, Pk));
        if (res) {
            Value kValue = res.value(state, ObjectPropertyName(state, k), thisObject);
            Value args[3] = { kValue, Pk, thisObject };
            Object::call(state, callbackfn, T, 3, args);
            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, thisObject, k, len, result);
            k = result;
            continue;
        }
    }
    return Value();
}

static Value builtinArrayIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
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

    double doubleK;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        doubleK = (n == -0) ? 0 : n;
    } else {
        // Else, n<0
        // Let k be len - abs(n).
        doubleK = len - std::abs(n);

        // If k is less than 0, then let k be 0.
        if (doubleK < 0) {
            doubleK = 0;
        }
    }

    ASSERT(doubleK >= 0);
    int64_t k = doubleK;

    // Repeat, while k<len
    while (k < len) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        auto kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
            Value elementK = kPresent.value(state, ObjectPropertyName(state, k), O);

            // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
            if (elementK.equalsTo(state, argv[0])) {
                // If same is true, return k.
                return Value(k);
            }
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            k = result;
            continue;
        }
        // Increase k by 1.
        k++;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinArrayLastIndexOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    double len = O->length(state);

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
    double doubleK;
    if (n >= 0) {
        doubleK = (n == -0) ? 0 : std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len - abs(n).
        doubleK = len - std::abs(n);
    }

    // NOTE in android arm32, -Inf to int64 generates wrong result
    if (doubleK < 0) {
        return Value(-1);
    }

    int64_t k = doubleK;
    // Repeat, while k≥ 0
    while (k >= 0) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        auto kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let elementK be the result of calling the [[Get]] internal method of O with the argument ToString(k).
            Value elementK = kPresent.value(state, ObjectPropertyName(state, k), O);

            // Let same be the result of applying the Strict Equality Comparison Algorithm to searchElement and elementK.
            if (elementK.equalsTo(state, argv[0])) {
                // If same is true, return k.
                return Value(k);
            }
        } else {
            int64_t result;
            Object::nextIndexBackward(state, O, k, -1, result);
            k = result;
            continue;
        }
        // Decrease k by 1.
        k--;
    }

    // Return -1.
    return Value(-1);
}

static Value builtinArrayEvery(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let k be 0.
    int64_t k = 0;

    while (k < len) {
        // Let Pk be ToString(k).
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        auto kPresent = O->hasIndexedProperty(state, Value(k));

        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, ObjectPropertyName(state, k), O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value args[] = { kValue, Value(k), O };
            Value testResult = Object::call(state, callbackfn, T, 3, args);

            if (!testResult.toBoolean()) {
                return Value(false);
            }

            // Increae k by 1.
            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            k = result;
        }
    }
    return Value(true);
}

static Value builtinArrayFill(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // Let relativeStart be ToInteger(start).
    double relativeStart = 0;
    if (argc > 1) {
        relativeStart = argv[1].toInteger(state);
    }

    // If relativeStart < 0, let k be max((len + relativeStart),0); else let k be min(relativeStart, len).
    int64_t k = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);

    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = len;
    if (argc > 2 && !argv[2].isUndefined()) {
        relativeEnd = argv[2].toInteger(state);
    }

    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    int64_t fin = (relativeEnd < 0) ? std::max(len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);

    Value value = argv[0];
    while (k < fin) {
        O->setIndexedPropertyThrowsException(state, Value(k), value);
        k++;
    }
    // return O.
    return O;
}

static Value builtinArrayFilter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let A be ArraySpeciesCreate(O, 0).
    Object* A = arraySpeciesCreate(state, O, 0);

    // Let k be 0.
    int64_t k = 0;
    // Let to be 0.
    int64_t to = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectHasPropertyResult kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, ObjectPropertyName(state, k), O);

            // Let selected be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value v[] = { kValue, Value(k), O };
            Value selected = Object::call(state, callbackfn, T, 3, v);

            // If ToBoolean(selected) is true, then
            if (selected.toBoolean()) {
                // Let status be CreateDataPropertyOrThrow (A, ToString(to), kValue).
                ASSERT(A != nullptr);
                A->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(to)), ObjectPropertyDescriptor(kValue, ObjectPropertyDescriptor::AllPresent));
                // Increase to by 1
                to++;
            }

            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            k = result;
        }
        // Increase k by 1.
    }

    // Return A.
    return A;
}

static Value builtinArrayMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().every.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    Value T;
    if (argc > 1)
        T = argv[1];

    // Let A be ArraySpeciesCreate(O, len).
    Object* A = arraySpeciesCreate(state, O, len);

    // Let k be 0.
    int64_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        auto kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            auto Pk = ObjectPropertyName(state, k);
            Value kValue = kPresent.value(state, Pk, O);
            // Let mappedValue be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value v[] = { kValue, Value(k), O };
            Value mappedValue = Object::call(state, callbackfn, T, 3, v);
            // Let status be CreateDataPropertyOrThrow (A, Pk, mappedValue).
            A->defineOwnPropertyThrowsException(state, Pk, ObjectPropertyDescriptor(mappedValue, ObjectPropertyDescriptor::AllPresent));
            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            k = result;
        }
        // Increase k by 1.
    }

    return A;
}

static Value builtinArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().some.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    Value T;
    // If thisArg was supplied, let T be thisArg; else let T be undefined.
    if (argc > 1) {
        T = argv[1];
    }

    // Let k be 0.
    int64_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ToString(k).
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectHasPropertyResult kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            ObjectPropertyName Pk(state, k);
            Value kValue = kPresent.value(state, Pk, O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value argv[] = { kValue, Value(k), O };
            Value testResult = Object::call(state, callbackfn, T, 3, argv);
            // If ToBoolean(testResult) is true, return true.
            if (testResult.toBoolean()) {
                return Value(true);
            }
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
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
static Value builtinArrayIncludes(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let len be ? ToLength(? Get(O, "length")).
    int64_t len = O->length(state);

    // If len is 0, return false.
    if (len == 0) {
        return Value(false);
    }

    Value searchElement = argv[0];
    // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step produces the value 0.)
    double n = argc >= 2 ? argv[1].toInteger(state) : 0;
    double doubleK;
    // If n ≥ 0, then
    if (n >= 0) {
        // Let k be n.
        doubleK = n;
    } else {
        // Else n < 0,
        // Let k be len + n.
        doubleK = len + n;
    }

    // If k < 0, let k be 0.
    if (doubleK < 0) {
        doubleK = 0;
    }

    ASSERT(doubleK >= 0);

    // Repeat, while k < len
    while (doubleK < len) {
        // Let elementK be the result of ? Get(O, ! ToString(k)).
        Value elementK = O->get(state, ObjectPropertyName(state, Value(Value::DoubleToIntConvertibleTestNeeds, doubleK))).value(state, O);
        // If SameValueZero(searchElement, elementK) is true, return true.
        if (elementK.equalsToByTheSameValueZeroAlgorithm(state, searchElement)) {
            return Value(true);
        }
        // Increase k by 1.
        doubleK++;
    }

    // Return false.
    return Value(false);
}

static Value builtinArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-402/6.0/index.html#sup-array.prototype.tolocalestring
    // Let array be the result of calling ToObject passing the this value as the argument.
    Object* array = thisValue.toObject(state);

    if (!state.context()->toStringRecursionPreventer()->canInvokeToString(array)) {
        return String::emptyString;
    }
    ToStringRecursionPreventerItemAutoHolder holder(state, array);

    // Let len be ? ToLength(? Get(array, "length")).
    uint64_t len = array->length(state);

    // Let separator be the String value for the list-separator String appropriate for the host environment’s current locale (this is derived in an implementation-defined way).
    String* separator = state.context()->staticStrings().asciiTable[(size_t)','].string();

    // Let R be the empty String.
    String* R = String::emptyString;

    // Let k be 0.
    uint64_t k = 0;

    // Repeat, while k < len
    while (k < len) {
        // If k > 0, then
        if (k > 0) {
            // Set R to the string-concatenation of R and separator.
            StringBuilder builder;
            builder.appendString(R);
            builder.appendString(separator);
            R = builder.finalize(&state);
        }
        // Let nextElement be ? Get(array, ! ToString(k)).
        Value nextElement = array->getIndexedPropertyValue(state, Value(k), array);
        // If nextElement is not undefined or null, then
        if (!nextElement.isUndefinedOrNull()) {
            // Let S be ? ToString(? Invoke(nextElement, "toLocaleString", « locales, options »)).
            Value func = nextElement.toObject(state)->get(state, state.context()->staticStrings().toLocaleString).value(state, nextElement);
            String* S = Object::call(state, func, nextElement, argc, argv).toString(state);
            // Set R to the string-concatenation of R and S.
            StringBuilder builder;
            builder.appendString(R);
            builder.appendString(S);
            R = builder.finalize(&state);
        }
        // Increase k by 1.
        k++;
    }

    // Return R.
    return R;
}

static Value builtinArrayReduce(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    int64_t len = O->length(state); // 2-3
    Value callbackfn = argv[0];
    Value initialValue = Value(Value::EmptyValue);
    if (argc > 1) {
        initialValue = argv[1];
    }

    if (!callbackfn.isCallable()) // 4
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);

    if (len == 0 && (initialValue.isUndefined() || initialValue.isEmpty())) // 5
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), ErrorObject::Messages::GlobalObject_ReduceError);

    int64_t k = 0; // 6
    Value accumulator;
    if (!initialValue.isEmpty()) { // 7
        accumulator = initialValue;
    } else { // 8
        ObjectHasPropertyResult kPresent; // 8.a
        while (!kPresent && k < len) { // 8.b
            kPresent = O->hasIndexedProperty(state, Value(k)); // 8.b.ii
            if (kPresent) {
                accumulator = kPresent.value(state, ObjectPropertyName(state, k), O);
            }
            k++; // 8.b.iv
        }
        if (!kPresent)
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduce.string(), ErrorObject::Messages::GlobalObject_ReduceError);
    }
    while (k < len) { // 9
        ObjectHasPropertyResult kPresent = O->hasIndexedProperty(state, Value(k)); // 9.b
        if (kPresent) { // 9.c
            Value kValue = kPresent.value(state, ObjectPropertyName(state, k), O); // 9.c.i
            const int fnargc = 4;
            Value fnargs[] = { accumulator, kValue, Value(k), O };
            accumulator = Object::call(state, callbackfn, Value(), fnargc, fnargs);
            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            k = result;
        }
    }
    return accumulator;
}

static Value builtinArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let lenValue be the result of calling the [[Get]] internal method of O with the argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // If IsCallable(callbackfn) is false, throw a TypeError exception.
    Value callbackfn = argv[0];
    if (!callbackfn.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true,
                                       state.context()->staticStrings().reduceRight.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // If len is 0 and initialValue is not present, throw a TypeError exception.
    if (len == 0 && argc < 2) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduceRight.string(), ErrorObject::Messages::GlobalObject_ReduceError);
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
        ObjectHasPropertyResult kPresent;

        // Repeat, while kPresent is false and k ≥ 0
        while (!kPresent && k >= 0) {
            // Let Pk be ToString(k).
            // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
            kPresent = O->hasIndexedProperty(state, Value(k));

            // If kPresent is true, then
            if (kPresent) {
                // Let accumulator be the result of calling the [[Get]] internal method of O with argument Pk.
                accumulator = kPresent.value(state, ObjectPropertyName(state, k), O);
            }

            // Decrease k by 1.
            int64_t result;
            Object::nextIndexBackward(state, O, k, -1, result);
            k = result;
        }
        // If kPresent is false, throw a TypeError exception.
        if (!kPresent) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().reduceRight.string(), ErrorObject::Messages::GlobalObject_ReduceError);
        }
    }

    // Repeat, while k ≥ 0
    while (k >= 0) {
        // Let Pk be ToString(k).
        ObjectPropertyName Pk(state, k);
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument Pk.
        ObjectHasPropertyResult kPresent = O->hasIndexedProperty(state, Value(k));
        // If kPresent is true, then
        if (kPresent) {
            // Let kValue be the result of calling the [[Get]] internal method of O with argument Pk.
            Value kValue = kPresent.value(state, ObjectPropertyName(state, k), O);

            // Let accumulator be the result of calling the [[Call]] internal method of callbackfn with undefined as the this value and argument list containing accumulator, kValue, k, and O.
            Value v[] = { accumulator, kValue, Value(k), O };
            accumulator = Object::call(state, callbackfn, Value(), 4, v);
        }

        // Decrease k by 1.
        int64_t result;
        Object::nextIndexBackward(state, O, k, -1, result);
        k = result;
    }

    // Return accumulator.
    return accumulator;
}

static Value builtinArrayPop(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToUint32(lenVal).
    int64_t len = O->length(state);

    // If len is zero,
    if (len == 0) {
        // Call the [[Put]] internal method of O with arguments "length", 0, and true.
        O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(0), O);
        // Return undefined.
        return Value();
    } else {
        // Else, len > 0
        // Let indx be ToString(len–1).
        ObjectPropertyName indx(state, len - 1);
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

static Value builtinArrayPush(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Array.prototype.push ( [ item1 [ , item2 [ , … ] ] ] )
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t n = O->length(state);

    // If len + argCount > 2^53 - 1, throw a TypeError exception.
    CHECK_ARRAY_LENGTH((uint64_t)n + argc > Value::maximumLength());

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

// https://www.ecma-international.org/ecma-262/10.0/#sec-array.prototype.flat
// Array.prototype.flat( [ depth ] )
static Value builtinArrayFlat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    int64_t sourceLen = O->length(state);
    double depthNum = 1;
    if (argc > 0 && !argv[0].isUndefined()) {
        depthNum = argv[0].toInteger(state);
    }
    Object* A = arraySpeciesCreate(state, O, 0);
    flattenIntoArray(state, A, O, sourceLen, 0, depthNum);

    return A;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-array.prototype.flatmap
// Array.prototype.flatMap ( mapperFunction [ , thisArg ] )
static Value builtinArrayFlatMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* O = thisValue.toObject(state);
    int64_t sourceLen = O->length(state);
    if (!argv[0].isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().flatMap.string(), ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    Value t;
    if (argc > 1) {
        t = argv[1];
    }
    Object* A = arraySpeciesCreate(state, O, 0);
    flattenIntoArray(state, A, O, sourceLen, 0, 1, argv[0], t);
    return A;
}

static Value builtinArrayShift(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToLength(Get(O, "length")).
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
        ObjectPropertyName from(state, k);
        // Let to be ToString(k–1).
        ObjectPropertyName to(state, k - 1);
        // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        auto fromPresent = O->hasIndexedProperty(state, Value(k));

        // If fromPresent is true, then
        if (fromPresent) {
            // Let fromVal be the result of calling the [[Get]] internal method of O with argument from.
            Value fromVal = fromPresent.value(state, from, O);
            // Call the [[Put]] internal method of O with arguments to, fromVal, and true.
            O->setThrowsException(state, to, fromVal, O);
        } else {
            // Else, fromPresent is false
            // Call the [[Delete]] internal method of O with arguments to and true.
            O->deleteOwnPropertyThrowsException(state, to);
        }

        // Increase k by 1.
        if (fromPresent) {
            k++;
        } else {
            int64_t result;
            Object::nextIndexForward(state, O, k, len, result);
            int64_t r = result;
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

static Value builtinArrayUnshift(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the result of calling ToObject passing the this value as the argument.
    Object* O = thisValue.toObject(state);
    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let len be ToLength(Get(O, "length")).
    int64_t len = O->length(state);

    // Let argCount be the number of actual arguments.
    int64_t argCount = argc;
    // Let k be len.
    int64_t k = len;

    // If argCount > 0, then
    // this line add in newer version ECMAScript than ECMAScript 5.1
    // http://www.ecma-international.org/ecma-262/6.0/index.html#sec-array.prototype.unshift
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.4.4.13
    if (argCount) {
        // If len + argCount > 2^53 - 1, throw a TypeError exception.
        CHECK_ARRAY_LENGTH(len + argCount > Value::maximumLength());

        // Repeat, while k > 0,
        while (k > 0) {
            // Let from be ToString(k–1).
            // Let to be ToString(k+argCount –1).
            ObjectPropertyName to(state, k + argCount - 1);

            // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
            ObjectHasPropertyResult fromPresent = O->hasIndexedProperty(state, Value(k - 1));
            // If fromPresent is true, then
            if (fromPresent) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                ObjectPropertyName from(state, k - 1);
                Value fromValue = fromPresent.value(state, from, O);
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setThrowsException(state, to, fromValue, O);
            } else {
                // Else, fromPresent is false
                // Call the [[Delete]] internal method of O with arguments to, and true.
                O->deleteOwnPropertyThrowsException(state, to);
            }

            if (fromPresent) {
                // Decrease k by 1.
                k--;
            } else {
                int64_t result;
                Object::nextIndexBackward(state, O, k, -1, result);
                int64_t r = std::max(result + 1, result - argCount + 1);
                if (r < k && std::abs(r - k) > argCount) {
                    k = r;
                } else {
                    k--;
                }
            }
        }

        // Let j be 0.
        int64_t j = 0;
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
static Value builtinArrayFind(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let len be ? ToLength(? Get(O, "length")).
    uint64_t len = O->length(state);
    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().find.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    uint64_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->get(state, ObjectPropertyName(state, Value(k))).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, thisArg, « kValue, k, O »)).
        Value v[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, argv[0], thisArg, 3, v).toBoolean();
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
static Value builtinArrayFindIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let len be ? ToLength(? Get(O, "length")).
    uint64_t len = O->length(state);
    // If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().findIndex.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }

    // Let k be 0.
    uint64_t k = 0;
    // Repeat, while k < len
    while (k < len) {
        // Let Pk be ! ToString(k).
        // Let kValue be ? Get(O, Pk).
        Value kValue = O->get(state, ObjectPropertyName(state, Value(k))).value(state, O);
        // Let testResult be ToBoolean(? Call(predicate, thisArg, « kValue, k, O »)).
        Value v[] = { kValue, Value(k), O };
        bool testResult = Object::call(state, argv[0], thisArg, 3, v).toBoolean();
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

// Array.prototype.copyWithin (target, start [ , end ] )
static Value builtinArrayCopyWithin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    Object* O = thisValue.toObject(state);
    // Let len be ToLength(Get(O, "length")).
    double len = O->length(state);
    // Let relativeTarget be ToInteger(target).
    double relativeTarget = argv[0].toInteger(state);
    // If relativeTarget < 0, let to be max((len + relativeTarget),0); else let to be min(relativeTarget, len).
    double to = (relativeTarget < 0.0) ? std::max((len + relativeTarget), 0.0) : std::min(relativeTarget, len);
    // Let relativeStart be ToInteger(start).
    double relativeStart = argv[1].toInteger(state);
    // If relativeStart < 0, let from be max((len + relativeStart),0); else let from be min(relativeStart, len).
    double from = (relativeStart < 0.0) ? std::max((len + relativeStart), 0.0) : std::min(relativeStart, len);
    // If end is undefined, let relativeEnd be len; else let relativeEnd be ToInteger(end).
    double relativeEnd = (argc < 3 || argv[2].isUndefined()) ? len : argv[2].toInteger(state);
    // If relativeEnd < 0, let final be max((len + relativeEnd),0); else let final be min(relativeEnd, len).
    double finalEnd = (relativeEnd < 0.0) ? std::max((len + relativeEnd), 0.0) : std::min(relativeEnd, len);
    // Let count be min(final-from, len-to).
    double count = std::min(finalEnd - from, len - to);
    int8_t direction;
    // If from<to and to<from+count
    if (from < to && to < from + count) {
        // Let direction be -1.
        direction = -1;
        // Let from be from + count -1.
        from = from + count - 1;
        // Let to be to + count -1.
        to = to + count - 1;
    } else {
        // Let direction = 1.
        direction = 1;
    }

    int64_t intCount = count;
    int64_t intFrom = from;
    int64_t intTo = to;

    // Repeat, while count > 0
    while (intCount > 0) {
        // Let fromPresent be HasProperty(O, fromKey).
        ObjectHasPropertyResult fromValue = O->hasIndexedProperty(state, Value(intFrom));
        // If fromPresent is true, then
        if (fromValue) {
            // Let setStatus be Set(O, toKey, fromVal, true).
            O->setIndexedPropertyThrowsException(state, Value(intTo), fromValue.value(state, ObjectPropertyName(state, intFrom), O));
        } else {
            // Let deleteStatus be DeletePropertyOrThrow(O, toKey).
            O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(intTo)));
        }
        // Let from be from + direction.
        intFrom += direction;
        // Let to be to + direction.
        intTo += direction;
        // Let count be count − 1.
        intCount--;
    }
    // Return O.
    return O;
}

static Value builtinArrayWith(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be ToObject(this value).
    Object* O = thisValue.toObject(state);

    uint64_t len = O->length(state);
    double relativeIndex = argv[0].toInteger(state);
    double actualIndex = relativeIndex;

    if (relativeIndex < 0) {
        actualIndex = len + relativeIndex;
    }

    if (UNLIKELY(actualIndex >= len || actualIndex < 0)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, ErrorObject::Messages::GlobalObject_InvalidArrayBufferOffset);
    }

    ArrayObject* arr = new ArrayObject(state, len);

    uint64_t k = 0;
    Value fromValue;
    while (k < len) {
        if (k == actualIndex) {
            fromValue = argv[1];
        } else {
            fromValue = O->getIndexedPropertyValue(state, Value(k), O);
        }

        arr->setIndexedProperty(state, Value(k), fromValue, arr);
        k++;
    }

    return arr;
}

static Value builtinArrayKeys(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* M = thisValue.toObject(state);
    return M->keys(state);
}

static Value builtinArrayValues(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* M = thisValue.toObject(state);
    return M->values(state);
}

static Value builtinArrayEntries(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* M = thisValue.toObject(state);
    return M->entries(state);
}

static Value builtinArrayIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (!thisValue.isObject() || !thisValue.asObject()->isArrayIteratorObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().ArrayIterator.string(), true, state.context()->staticStrings().next.string(), ErrorObject::Messages::GlobalObject_CalledOnIncompatibleReceiver);
    }

    ArrayIteratorObject* iter = thisValue.asObject()->asIteratorObject()->asArrayIteratorObject();
    return iter->next(state);
}

static Value builtinArrayAt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* obj = thisValue.toObject(state);
    size_t len = obj->length(state);
    double relativeIndex = argv[0].toInteger(state);
    double k = (relativeIndex < 0) ? len + relativeIndex : relativeIndex;

    if (UNLIKELY(k < 0 || k >= len)) {
        return Value();
    }

    return obj->getIndexedPropertyValue(state, Value(Value::DoubleToIntConvertibleTestNeeds, k), thisValue);
}

static Value builtinArrayFindLast(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/proposal-array-find-from-last/index.html#sec-array.prototype.findlast
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // 1. Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // 2. Let len be ? LengthOfArrayLike(O).
    int64_t len = static_cast<int64_t>(O->length(state));
    // 3. If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().findLast.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    // 4. Let k be len - 1.
    int64_t k = len - 1;
    // 5. Repeat, while k ≥ 0,
    while (k >= 0) {
        // a. Let Pk be ! ToString(𝔽(k)).
        // b. Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
        // c. Let testResult be ! ToBoolean(? Call(predicate, thisArg, « kValue, 𝔽(k), O »)).
        Value predicateArgv[] = {
            kValue, Value(k), Value(O)
        };
        bool testResult = Object::call(state, predicate, thisArg, 3, predicateArgv).toBoolean();
        // d. If testResult is true, return kValue.
        if (testResult) {
            return kValue;
        }
        // e. Set k to k - 1.
        k--;
    }

    return Value();
}

static Value builtinArrayFindLastIndex(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/proposal-array-find-from-last/index.html#sec-array.prototype.findlastindex
    Value predicate = argv[0];
    Value thisArg = argc > 1 ? argv[1] : Value();
    // 1. Let O be ? ToObject(this value).
    Object* O = thisValue.toObject(state);
    // 2. Let len be ? LengthOfArrayLike(O).
    int64_t len = static_cast<int64_t>(O->length(state));
    // 3. If IsCallable(predicate) is false, throw a TypeError exception.
    if (!predicate.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().findLastIndex.string(), ErrorObject::Messages::GlobalObject_CallbackNotCallable);
    }
    // 4. Let k be len - 1.
    int64_t k = len - 1;
    // 5. Repeat, while k ≥ 0,
    while (k >= 0) {
        // a. Let Pk be ! ToString(𝔽(k)).
        // b. Let kValue be ? Get(O, Pk).
        Value kValue = O->getIndexedPropertyValue(state, Value(k), O);
        // c. Let testResult be ! ToBoolean(? Call(predicate, thisArg, « kValue, 𝔽(k), O »)).
        Value predicateArgv[] = {
            kValue, Value(k), Value(O)
        };
        bool testResult = Object::call(state, predicate, thisArg, 3, predicateArgv).toBoolean();
        // d. If testResult is true, return kValue.
        if (testResult) {
            return Value(k);
        }
        // e. Set k to k - 1.
        k--;
    }

    return Value(-1);
}

void GlobalObject::initializeArray(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->array();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Array), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installArray(ExecutionState& state)
{
    ASSERT(!!m_iteratorPrototype);
    ASSERT(!!m_arrayToString);

    m_array = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_array->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_array->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_arrayPrototype = new ArrayPrototypeObject(state);
    m_arrayPrototype->setGlobalIntrinsicObject(state, true);

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isArray),
                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isArray, builtinArrayIsArray, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().from),
                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().from, builtinArrayFrom, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().of),
                                     ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().of, builtinArrayOf, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinArrayConcat, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinArrayForEach, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().indexOf),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().indexOf, builtinArrayIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lastIndexOf),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().lastIndexOf, builtinArrayLastIndexOf, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().join),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().join, builtinArrayJoin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sort),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sort, builtinArraySort, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().splice),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().splice, builtinArraySplice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().slice),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().slice, builtinArraySlice, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toReversed),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toReversed, builtinArrayToReversed, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toSorted),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toSorted, builtinArrayToSorted, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toSpliced),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toSpliced, builtinArrayToSpliced, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().every),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().every, builtinArrayEvery, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().fill),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().fill, builtinArrayFill, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().includes),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().includes, builtinArrayIncludes, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().filter),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().filter, builtinArrayFilter, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().reduce),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reduce, builtinArrayReduce, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().reduceRight),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reduceRight, builtinArrayReduceRight, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().pop),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().pop, builtinArrayPop, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().push),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().push, builtinArrayPush, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().shift),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().shift, builtinArrayShift, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().reverse),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reverse, builtinArrayReverse, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                              ObjectPropertyDescriptor(m_arrayToString, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().map),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().map, builtinArrayMap, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().some),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().some, builtinArraySome, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toLocaleString),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toLocaleString, builtinArrayToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().unshift),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().unshift, builtinArrayUnshift, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().keys),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().keys, builtinArrayKeys, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().find),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().find, builtinArrayFind, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findIndex),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().findIndex, builtinArrayFindIndex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().copyWithin),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().copyWithin, builtinArrayCopyWithin, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().with),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().with, builtinArrayWith, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().flat),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().flat, builtinArrayFlat, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().flatMap),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().flatMap, builtinArrayFlatMap, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().at),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().at, builtinArrayAt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findLast),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().findLast, builtinArrayFindLast, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findLastIndex),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().findLastIndex, builtinArrayFindLastIndex, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    Object* blackList = new Object(state, Object::PrototypeIsNull);
    blackList->markThisObjectDontNeedStructureTransitionTable();
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().at), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().copyWithin), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().entries), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().fill), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().find), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findLast), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findLastIndex), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().findIndex), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().keys), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().values), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().includes), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().flat), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().flatMap), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toReversed), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toSorted), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));
    blackList->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().toSpliced), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));

    FunctionObject* values = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().values, builtinArrayValues, 0, NativeFunctionInfo::Strict));
    // Well-Known Intrinsic Objects : %ArrayProto_values%
    // The initial value of the values data property of %ArrayPrototype%
    m_arrayPrototypeValues = values;
    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().values),
                                              ObjectPropertyDescriptor(values, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().iterator),
                                              ObjectPropertyDescriptor(values,
                                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().entries),
                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().entries, builtinArrayEntries, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().unscopables),
                                              ObjectPropertyDescriptor(blackList, ObjectPropertyDescriptor::ConfigurablePresent));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    m_arrayIteratorPrototype = new PrototypeObject(state, m_iteratorPrototype);
    m_arrayIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_arrayIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinArrayIteratorNext, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                      ObjectPropertyDescriptor(Value(String::fromASCII("Array Iterator")), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                        ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
