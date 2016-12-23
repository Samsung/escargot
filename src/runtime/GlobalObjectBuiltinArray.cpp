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

    array->setArrayLength(state, size, true);
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


static Value builtinArrayJoin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisBinded, Array, join);
    uint32_t len = thisBinded->length(state);
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
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Array, splice);
    uint32_t len = thisObject->length(state);
    double relativeStart = argv[0].toInteger(state);
    uint32_t actualStart = (relativeStart < 0) ? std::max(len + relativeStart, 0.0) : std::min(relativeStart, (double)len);
    // TODO(ES6): the number of actual arguments is used.
    // e.g. var arr = [1, 2, 3, 4, 5];
    //      Different: arr.splice(2) vs. arr.splice(2, undefined)

    uint32_t insertCnt = (argc >= 2) ? argc - 2 : 0;
    uint32_t deleteCnt = std::min(std::max(argv[1].toInteger(state), 0.0), (double)len - actualStart);
    double finalCnt = (double)len - deleteCnt + insertCnt;
    if (finalCnt > Value::InvalidArrayIndexValue) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_RangeError);
    }

    uint32_t k = 0;
    ArrayObject* array = new ArrayObject(state);
    while (k < deleteCnt) {
        ObjectGetResult fromValue = thisObject->get(state, ObjectPropertyName(state, Value(actualStart + k)));
        if (fromValue.hasValue()) {
            array->defineOwnProperty(state, ObjectPropertyName(state, Value(k)),
                                     ObjectPropertyDescriptor(fromValue.value(state, thisObject), ObjectPropertyDescriptor::AllPresent));
            k++;
        } else {
            k = Object::nextIndexForward(state, thisObject, k, len, false);
        }
    }

    if (insertCnt < deleteCnt) {
        k = actualStart;
        // move [actualStart + deleteCnt, len) to [actualStart + insertCnt, len - deleteCnt + insertCnt)
        while (k < len - deleteCnt) {
            // TODO: thisObject->relocateIndexesForward((double)start + deleteCnt, len, insertCnt - deleteCnt);
            uint32_t from = k + deleteCnt;
            uint32_t to = k + insertCnt;
            ObjectGetResult fromValue = thisObject->get(state, ObjectPropertyName(state, Value(from)));
            if (fromValue.hasValue()) {
                thisObject->setThrowsException(state, ObjectPropertyName(state, Value(to)), fromValue.value(state, thisObject), thisObject);
            } else {
                thisObject->deleteOwnProperty(state, ObjectPropertyName(state, Value(to)));
            }
            k++;
        }
        // delete [len - deleteCnt + insertCnt, len)
        k = len;
        while (k > len - deleteCnt + insertCnt) {
            thisObject->deleteOwnProperty(state, ObjectPropertyName(state, Value(k - 1)));
            k--;
        }
    } else if (insertCnt > deleteCnt) {
        // TODO
        RELEASE_ASSERT_NOT_REACHED();
    }

    k = actualStart;
    if (insertCnt > 0) {
        // TODO: insert items
        RELEASE_ASSERT_NOT_REACHED();
    }

    thisObject->setThrowsException(state, ObjectPropertyName(state.context()->staticStrings().length), Value(finalCnt), thisObject);
    return array;
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
            uint32_t len = arr->length(state);
            if (n > Value::InvalidArrayIndexValue - len) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_RangeError);
            }

            uint32_t curIndex = 0;
            while (curIndex < len) {
                ObjectGetResult exists = arr->get(state, ObjectPropertyName(state, Value(curIndex)));
                if (exists.hasValue()) {
                    array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n + curIndex)), ObjectPropertyDescriptor(exists.value(state, arr), ObjectPropertyDescriptor::AllPresent));
                    curIndex++;
                } else {
                    curIndex = Object::nextIndexForward(state, arr, curIndex, len, false);
                }
            }
            n += len;
        } else {
            if (n > Value::InvalidArrayIndexValue - 1) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, errorMessage_GlobalObject_RangeError);
            }
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n++)), ObjectPropertyDescriptor(argi, ObjectPropertyDescriptor::AllPresent));
        }
    }
    array->setLength(state, n);
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
    array->setLength(state, n);
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

static Value builtinArrayPush(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Array.prototype.push ( [ item1 [ , item2 [ , … ] ] ] )
    // Let O be the result of calling ToObject passing the this value as the argument.
    RESOLVE_THIS_BINDING_TO_OBJECT(O, Array, push);

    // Let lenVal be the result of calling the [[Get]] internal method of O with argument "length".
    // Let n be ToUint32(lenVal).
    uint32_t n = O->length(state);
    // Let items be an internal List whose elements are, in left to right order, the arguments that were passed to this function invocation.
    // Repeat, while items is not empty
    // Remove the first element from items and let E be the value of the element.
    for (size_t i = 0; i < argc; i++) {
        // Call the [[Put]] internal method of O with arguments ToString(n), E, and true.
        O->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, Value(n)), argv[i], O);
        // Increase n by 1.
        n++;
        // Call the [[Put]] internal method of O with arguments "length", n, and true.
        O->setThrowsExceptionWhenStrictMode(state, ObjectPropertyName(state, state.context()->staticStrings().length), argv[i], O);
    }

    // Return n.
    return Value(n);
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
    m_arrayPrototype->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().constructor), ObjectPropertyDescriptor(m_array));

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinArrayConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().forEach),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().forEach, builtinArrayForEach, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
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
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().reduce),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reduce, builtinArrayReduce, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().push),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().push, builtinArrayPush, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().reverse),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().reverse, builtinArrayReverse, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                       ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinArrayToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      ObjectPropertyDescriptor(m_array, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
