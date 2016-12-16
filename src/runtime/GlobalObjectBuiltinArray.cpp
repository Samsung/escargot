#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "ArrayObject.h"

namespace Escargot {

static Value builtinArrayConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

    array->setLength(state, size);
    if (interpretArgumentsAsElements) {
        Value val = argv[0];
        if (argc > 1 || !val.isInt32()) {
            for (size_t idx = 0; idx < argc; idx++) {
                array->defineOwnProperty(state, ObjectPropertyName(state, Value(idx)), ObjectPropertyDescriptorForDefineOwnProperty(val, ObjectPropertyDescriptorForDefineOwnProperty::AllPresent));
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
        Value elem = thisBinded->get(state, ObjectPropertyName(state, Value(curIndex))).value();

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
                                     ObjectPropertyDescriptorForDefineOwnProperty(fromValue.value(), ObjectPropertyDescriptorForDefineOwnProperty::AllPresent));
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
                thisObject->setThrowsException(state, ObjectPropertyName(state, Value(to)), fromValue.value(), thisObject);
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
    Value toString = thisObject->get(state, state.context()->staticStrings().join).value();
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
                    array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n + curIndex)), ObjectPropertyDescriptorForDefineOwnProperty(exists.value(), ObjectPropertyDescriptorForDefineOwnProperty::AllPresent));
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
            array->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(n++)), ObjectPropertyDescriptorForDefineOwnProperty(argi, ObjectPropertyDescriptorForDefineOwnProperty::AllPresent));
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
                                                    ObjectPropertyDescriptorForDefineOwnProperty(exists.value(), ObjectPropertyDescriptorForDefineOwnProperty::AllPresent));
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

void GlobalObject::installArray(ExecutionState& state)
{
    m_array = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().Array, builtinArrayConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                     return new ArrayObject(state);
                                 }));
    m_array->markThisObjectDontNeedStructureTransitionTable(state);
    m_array->setPrototype(state, m_functionPrototype);
    // TODO m_array->defineAccessorProperty(strings->prototype.string(), ESVMInstance::currentInstance()->functionPrototypeAccessorData(), false, false, false);
    m_arrayPrototype = m_objectPrototype;
    m_arrayPrototype = new ArrayObject(state);
    m_arrayPrototype->setPrototype(state, m_objectPrototype);

    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().concat),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().concat, builtinArrayConcat, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().join),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().join, builtinArrayJoin, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sort),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sort, builtinArraySort, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().splice),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().splice, builtinArraySplice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().slice),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().slice, builtinArraySlice, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
    m_arrayPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().toString),
                                                       ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().toString, builtinArrayToString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));

    m_array->setFunctionPrototype(state, m_arrayPrototype);

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Array),
                      ObjectPropertyDescriptorForDefineOwnProperty(m_array, (ObjectPropertyDescriptorForDefineOwnProperty::PresentAttribute)(ObjectPropertyDescriptorForDefineOwnProperty::WritablePresent | ObjectPropertyDescriptorForDefineOwnProperty::ConfigurablePresent)));
}
}
