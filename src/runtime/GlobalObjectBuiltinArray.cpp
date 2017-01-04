#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ArrayObject.h"

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

    array->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(size), array);
    if (interpretArgumentsAsElements) {
        Value val = argv[0];
        if (argc > 1 || !val.isInt32()) {
            for (size_t idx = 0; idx < argc; idx++) {
                array->defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), ObjectPropertyDescriptor(val, ObjectPropertyDescriptor::AllPresent));
                val = argv[idx + 1];
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

static Value builtinArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, Array, join);
    int64_t len = thisBinded->length(state);
    Value separator = argv[0];
    // TODO
    // size_t lenMax = ESString::maxLength();
    size_t lenMax = std::numeric_limits<uint32_t>::max();
    String* sep;

    if (separator.isUndefined()) {
        sep = state.context()->staticStrings().asciiTable[(size_t)','].string();
    } else {
        sep = separator.toString(state);
    }
    // TODO
    /*
    StringRecursionChecker checker(thisBinded);
    if (checker.recursionCheck()) {
        return ESValue(strings->emptyString.string());
    }
*/
    StringBuilder builder;
    double prevIndex = 0;
    double curIndex = 0;
    while (curIndex < len) {
        if (curIndex != 0) {
            if (sep->length() > 0) {
                if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - (curIndex - prevIndex - 1) * sep->length())) {
                    RELEASE_ASSERT_NOT_REACHED();
                    // TODO
                    // instance->throwOOMError();
                }
                while (curIndex - prevIndex > 1) {
                    builder.appendString(sep);
                    prevIndex++;
                }
                builder.appendString(sep);
            }
        }
        Value elem = thisBinded->get(state, ObjectPropertyName(state, Value(curIndex))).value(state, thisBinded);

        if (!elem.isUndefinedOrNull()) {
            builder.appendString(elem.toString(state));
        }
        prevIndex = curIndex;
        if (elem.isUndefined()) {
            curIndex = Object::nextIndexForward(state, thisBinded, prevIndex, len, true);
        } else {
            curIndex++;
        }
    }
    if (sep->length() > 0) {
        if (static_cast<double>(builder.contentLength()) > static_cast<double>(lenMax - (curIndex - prevIndex - 1) * sep->length())) {
            // TODO
            RELEASE_ASSERT_NOT_REACHED();
            // instance->throwOOMError();
        }
        while (curIndex - prevIndex > 1) {
            builder.appendString(sep);
            prevIndex++;
        }
    }
    return builder.finalize();
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
            lowerValue = O->get(state, lowerP, O).value(state, O);
        }
        bool upperExists = O->hasProperty(state, upperP);
        Value upperValue;
        if (upperExists) {
            upperValue = O->get(state, upperP, O).value(state, O);
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
            unsigned nextLower = Object::nextIndexForward(state, O, lower, middle, false);
            unsigned nextUpper = Object::nextIndexBackward(state, O, upper, middle, false);
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
    if (!cmpfn.isUndefined()) {
        if (!cmpfn.isFunction()) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().sort.string(), errorMessage_GlobalObject_FirstArgumentNotCallable);
        }
    }
    bool defaultSort = (argc == 0) || cmpfn.isUndefined();

    thisObject->sort(state, [defaultSort, &cmpfn, &state, &thisObject](const Value& a, const Value& b) -> bool {
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
            Value ret = FunctionObject::call(cmpfn, state, Value(), 2, arg);
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
    int64_t actualDeleteCount = std::min(std::max(argv[1].toInteger(state), 0.0), (double)len - actualStart);

    // Let k be 0.
    int64_t k = 0;

    // Repeat, while k < actualDeleteCount
    while (k < actualDeleteCount) {
        // Let from be ToString(actualStart+k).
        ObjectPropertyName from(state, Value(actualStart + k));
        // Let fromPresent be the result of calling the [[HasProperty]] internal method of O with argument from.
        // If fromPresent is true, then
        // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
        ObjectGetResult fromValue = O->get(state, from);
        if (fromValue.hasValue()) {
            // Call the [[DefineOwnProperty]] internal method of A with arguments ToString(k), Property Descriptor {[[Value]]: fromValue, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            A->defineOwnProperty(state, ObjectPropertyName(state, Value(k)),
                                 ObjectPropertyDescriptor(fromValue.value(state, O), ObjectPropertyDescriptor::AllPresent));
            // Increment k by 1.
        }
        k = Object::nextIndexForward(state, O, k, len, false);
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
            ObjectGetResult fromValue = O->get(state, ObjectPropertyName(state, Value(from)));
            // If fromPresent is true, then
            if (fromValue.hasValue()) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                O->setThrowsException(state, ObjectPropertyName(state, Value(to)), fromValue.value(state, O), O);
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
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
            ObjectGetResult fromValue = O->get(state, from);
            // If fromPresent is true, then
            if (fromValue.hasValue()) {
                // Let fromValue be the result of calling the [[Get]] internal method of O with argument from.
                // Call the [[Put]] internal method of O with arguments to, fromValue, and true.
                O->setThrowsException(state, to, fromValue.value(state, O), O);
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
        O->setThrowsException(state, ObjectPropertyName(state, Value(k)), E, O);
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
    return FunctionObject::call(toString, state, thisObject, 0, nullptr);
}

static Value builtinArrayConcat(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, concat);
    ArrayObject* array = new ArrayObject(state);
    uint32_t n = 0;
    for (size_t i = 0; i < argc + 1; i++) {
        Value argi = (i == 0) ? thisObject : argv[i - 1];
        if (argi.isObject() && argi.asObject()->isArrayObject()) {
            ArrayObject* arr = argi.asObject()->asArrayObject();

            // Let k be 0.
            uint64_t k = 0;
            // Let len be the result of calling the [[Get]] internal method of E with argument "length".
            uint64_t len = arr->length(state);

            // Repeat, while k < len
            while (k < len) {
                // Let exists be the result of calling the [[HasProperty]] internal method of E with P.
                ObjectGetResult exists = arr->get(state, ObjectPropertyName(state, Value(k)));
                if (exists.hasValue()) {
                    array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n + k)), ObjectPropertyDescriptor(exists.value(state, arr), ObjectPropertyDescriptor::AllPresent));
                }
                k = Object::nextIndexForward(state, arr, k, len, false);
            }

            n += len;
            array->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(n), array);
        } else {
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n++)), ObjectPropertyDescriptor(argi, ObjectPropertyDescriptor::AllPresent));
        }
    }

    return array;
}

static Value builtinArraySlice(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, concat);
    uint32_t len = thisObject->length(state);
    double relativeStart = argv[0].toInteger(state);
    uint32_t k = (relativeStart < 0) ? std::max((double)len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    double relativeEnd = (argv[1].isUndefined()) ? len : argv[1].toInteger(state);
    uint32_t finalEnd = (relativeEnd < 0) ? std::max((double)len + relativeEnd, 0.0) : std::min(relativeEnd, (double)len);

    uint32_t n = 0;
    ArrayObject* array = new ArrayObject(state);
    while (k < finalEnd) {
        ObjectGetResult exists = thisObject->get(state, ObjectPropertyName(state, Value(k)));
        if (exists.hasValue()) {
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n)),
                                                    ObjectPropertyDescriptor(exists.value(state, thisObject), ObjectPropertyDescriptor::AllPresent));
            k++;
            n++;
        } else {
            double tmp = Object::nextIndexForward(state, thisObject, k, len, false);
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

    Value thisArg = argv[1];
    uint32_t k = 0;
    while (k < len) {
        Value Pk = Value(k);
        auto res = thisObject->get(state, ObjectPropertyName(state, Pk));
        if (res.hasValue()) {
            Value kValue = res.value(state, thisObject);
            Value args[3] = { kValue, Pk, thisObject };
            callbackfn.asFunction()->call(state, thisArg, 3, args, false);
            k++;
        } else {
            k = Object::nextIndexForward(state, thisObject, k, len, false);
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
        k = n;
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
        ObjectGetResult kPresent = O->get(state, ObjectPropertyName(state, Value(k)));
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
            k = Object::nextIndexForward(state, O, k, len, false);
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
        k = std::min(n, len - 1.0);
    } else {
        // Else, n < 0
        // Let k be len - abs(n).
        k = len - std::abs(n);
    }

    // Repeat, while k≥ 0
    while (k >= 0) {
        // Let kPresent be the result of calling the [[HasProperty]] internal method of O with argument ToString(k).
        ObjectGetResult kPresent = O->get(state, ObjectPropertyName(state, Value(k)));
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
            k = Object::nextIndexBackward(state, O, k, -1, false);
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
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, forEach);
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
            Value kValue = O->get(state, ObjectPropertyName(state, pk), O).value(state, O);
            // Let testResult be the result of calling the [[Call]] internal method of callbackfn with T as the this value and argument list containing kValue, k, and O.
            Value args[] = { kValue, Value(k), O };
            Value testResult = callbackfn.asFunction()->call(state, T, 3, args);

            if (!testResult.toBoolean(state)) {
                return Value(false);
            }

            // Increae k by 1.
            k++;
        } else {
            k = Object::nextIndexForward(state, O, k, len, false);
        }
    }
    return Value(true);
}

static Value builtinArrayFilter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinArrayMap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinArraySome(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinArrayToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let array be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(array, Array, toLocaleString);

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
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLoacleStringNotCallable);
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
        String* S = builder.finalize();

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
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Array.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLoacleStringNotCallable);
            }
            // Let R be the result of calling the [[Call]] internal method of func providing elementObj as the this value and an empty arguments list.
            R = func.asFunction()->call(state, elementObj, 0, nullptr);
        }
        // Let R be a String value produced by concatenating S and R.
        StringBuilder builder2;
        builder2.appendString(S);
        builder2.appendString(R.toString(state));
        R = builder.finalize();
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
            accumulator = FunctionObject::call(callbackfn, state, Value(), fnargc, fnargs);
            k++;
        } else {
            k = Object::nextIndexForward(state, O, k, len, false);
        }
    }
    return accumulator;
}

static Value builtinArrayReduceRight(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    state.throwException(new ASCIIString(errorMessage_NotImplemented));
    RELEASE_ASSERT_NOT_REACHED();
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
        O->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, Value(n)), argv[i], O);
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
    Value first = O->get(state, ObjectPropertyName(state, Value(0)), O).value(state, O);
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
        k++;
    }
    // Call the [[Delete]] internal method of O with arguments ToString(len–1) and true.
    O->deleteOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(len - 1)));
    // Call the [[Put]] internal method of O with arguments "length", (len–1) , and true.
    O->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(len - 1), O);
    // Return first.
    return first;
}

static Value builtinArrayUnShift(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

        // Decrease k by 1.
        k--;
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

    // Call the [[Put]] internal method of O with arguments "length", len+argCount, and true.
    O->setThrowsException(state, state.context()->staticStrings().length, Value(len + argCount), O);

    // Return len+argCount.
    return Value(len + argCount);
}


void GlobalObject::installArray(ExecutionState& state)
{
    m_array = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                     return new ArrayObject(state);
                                 }),
                                 FunctionObject::__ForBuiltin__);
    m_array->markThisObjectDontNeedStructureTransitionTable(state);
    m_array->setPrototype(state, m_functionPrototype);
    m_arrayPrototype = m_objectPrototype;
    m_arrayPrototype = new ArrayObject(state);
    m_arrayPrototype->setPrototype(state, m_objectPrototype);
    m_arrayPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().isArray),
                                              ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isArray, builtinArrayIsArray, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinArrayConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinArrayForEach, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
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
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().unshift, builtinArrayUnShift, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
