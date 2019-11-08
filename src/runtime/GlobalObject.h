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

#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/FunctionObject.h"
#include "runtime/Object.h"
#include "runtime/GlobalRegExpFunctionObject.h"

namespace Escargot {

class FunctionObject;

#define RESOLVE_THIS_BINDING_TO_OBJECT(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                    \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                 \
    Object* NAME = thisValue.toObject(state);

#define RESOLVE_THIS_BINDING_TO_STRING(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                    \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                              \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), errorMessage_GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                 \
    String* NAME = thisValue.toString(state);

Value builtinSpeciesGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);

class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;
    friend class GlobalEnvironmentRecord;
    friend class IdentifierNode;

    explicit GlobalObject(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, false)
        , m_context(state.context())
        , m_object(nullptr)
        , m_objectPrototypeToString(nullptr)
        , m_objectCreate(nullptr)
        , m_objectFreeze(nullptr)
        , m_function(nullptr)
        , m_functionPrototype(nullptr)
        , m_iteratorPrototype(nullptr)
        , m_error(nullptr)
        , m_errorPrototype(nullptr)
        , m_referenceError(nullptr)
        , m_referenceErrorPrototype(nullptr)
        , m_typeError(nullptr)
        , m_typeErrorPrototype(nullptr)
        , m_rangeError(nullptr)
        , m_rangeErrorPrototype(nullptr)
        , m_syntaxError(nullptr)
        , m_syntaxErrorPrototype(nullptr)
        , m_uriError(nullptr)
        , m_uriErrorPrototype(nullptr)
        , m_evalError(nullptr)
        , m_evalErrorPrototype(nullptr)
        , m_string(nullptr)
        , m_stringPrototype(nullptr)
        , m_stringIteratorPrototype(nullptr)
        , m_number(nullptr)
        , m_numberPrototype(nullptr)
        , m_symbol(nullptr)
        , m_symbolPrototype(nullptr)
        , m_array(nullptr)
        , m_arrayPrototype(nullptr)
        , m_arrayIteratorPrototype(nullptr)
        , m_arrayPrototypeValues(nullptr)
        , m_boolean(nullptr)
        , m_booleanPrototype(nullptr)
        , m_date(nullptr)
        , m_datePrototype(nullptr)
        , m_regexp(nullptr)
        , m_regexpPrototype(nullptr)
        , m_regexpSplitMethod(nullptr)
        , m_regexpReplaceMethod(nullptr)
        , m_math(nullptr)
        , m_eval(nullptr)
        , m_throwTypeError(nullptr)
        , m_throwerGetterSetterData(nullptr)
        , m_stringProxyObject(nullptr)
        , m_numberProxyObject(nullptr)
        , m_booleanProxyObject(nullptr)
        , m_symbolProxyObject(nullptr)
        , m_json(nullptr)
        , m_jsonStringify(nullptr)
        , m_jsonParse(nullptr)
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
        , m_intl(nullptr)
        , m_intlCollator(nullptr)
        , m_intlDateTimeFormat(nullptr)
        , m_intlNumberFormat(nullptr)
#endif
        , m_promise(nullptr)
        , m_promisePrototype(nullptr)
        , m_proxy(nullptr)
        , m_reflect(nullptr)
        , m_arrayBuffer(nullptr)
        , m_arrayBufferPrototype(nullptr)
        , m_dataView(nullptr)
        , m_dataViewPrototype(nullptr)
        , m_typedArray(nullptr)
        , m_typedArrayPrototype(nullptr)
        , m_int8Array(nullptr)
        , m_int8ArrayPrototype(nullptr)
        , m_uint8Array(nullptr)
        , m_uint8ArrayPrototype(nullptr)
        , m_uint8ClampedArray(nullptr)
        , m_uint8ClampedArrayPrototype(nullptr)
        , m_int16Array(nullptr)
        , m_int16ArrayPrototype(nullptr)
        , m_uint16Array(nullptr)
        , m_uint16ArrayPrototype(nullptr)
        , m_int32Array(nullptr)
        , m_int32ArrayPrototype(nullptr)
        , m_uint32Array(nullptr)
        , m_uint32ArrayPrototype(nullptr)
        , m_float32Array(nullptr)
        , m_float32ArrayPrototype(nullptr)
        , m_float64Array(nullptr)
        , m_float64ArrayPrototype(nullptr)
        , m_map(nullptr)
        , m_mapPrototype(nullptr)
        , m_mapIteratorPrototype(nullptr)
        , m_set(nullptr)
        , m_setPrototype(nullptr)
        , m_setIteratorPrototype(nullptr)
        , m_weakMap(nullptr)
        , m_weakMapPrototype(nullptr)
        , m_weakSet(nullptr)
        , m_weakSetPrototype(nullptr)
        , m_generatorFunction(nullptr)
        , m_generator(nullptr)
        , m_generatorPrototype(nullptr)
        , m_asyncFunction(nullptr)
        , m_asyncFunctionPrototype(nullptr)
    {
        m_objectPrototype = Object::createBuiltinObjectPrototype(state);
        m_objectPrototype->markThisObjectDontNeedStructureTransitionTable();
        Object::setPrototype(state, m_objectPrototype);

        m_structure = m_structure->convertToNonTransitionStructure();
    }

    virtual bool isGlobalObject() const
    {
        return true;
    }

    void installBuiltins(ExecutionState& state)
    {
        installFunction(state);
        installObject(state);
        installIterator(state);
        installError(state);
        installSymbol(state);
        installString(state);
        installNumber(state);
        installBoolean(state);
        installArray(state);
        installMath(state);
        installDate(state);
        installRegExp(state);
        installJSON(state);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
        installIntl(state);
#endif
        installPromise(state);
        installProxy(state);
        installReflect(state);
        installDataView(state);
        installTypedArray(state);
        installMap(state);
        installSet(state);
        installWeakMap(state);
        installWeakSet(state);
        installGenerator(state);
        installAsyncFunction(state);
        installOthers(state);
    }

    void installFunction(ExecutionState& state);
    void installObject(ExecutionState& state);
    void installError(ExecutionState& state);
    void installSymbol(ExecutionState& state);
    void installString(ExecutionState& state);
    void installNumber(ExecutionState& state);
    void installBoolean(ExecutionState& state);
    void installArray(ExecutionState& state);
    void installMath(ExecutionState& state);
    void installDate(ExecutionState& state);
    void installRegExp(ExecutionState& state);
    void installJSON(ExecutionState& state);
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    void installIntl(ExecutionState& state);
#endif
    void installPromise(ExecutionState& state);
    void installProxy(ExecutionState& state);
    void installReflect(ExecutionState& state);
    void installDataView(ExecutionState& state);
    void installTypedArray(ExecutionState& state);
    template <typename TA, int elementSize, typename TypeAdaptor>
    FunctionObject* installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction);
    void installIterator(ExecutionState& state);
    void installMap(ExecutionState& state);
    void installSet(ExecutionState& state);
    void installWeakMap(ExecutionState& state);
    void installWeakSet(ExecutionState& state);
    void installGenerator(ExecutionState& state);
    void installAsyncFunction(ExecutionState& state);
    void installOthers(ExecutionState& state);

    Value eval(ExecutionState& state, const Value& arg);
    Value evalLocal(ExecutionState& state, const Value& arg, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool inWithOperation); // we get isInWithOperation as parameter because this affects bytecode

    virtual const char* internalClassProperty()
    {
        return "global";
    }

    FunctionObject* object()
    {
        return m_object;
    }
    Object* objectPrototype()
    {
        return m_objectPrototype;
    }
    FunctionObject* objectPrototypeToString()
    {
        return m_objectPrototypeToString;
    }
    FunctionObject* objectCreate()
    {
        return m_objectCreate;
    }
    FunctionObject* objectFreeze()
    {
        return m_objectFreeze;
    }

    FunctionObject* function()
    {
        return m_function;
    }
    FunctionObject* functionPrototype()
    {
        return m_functionPrototype;
    }

    FunctionObject* error()
    {
        return m_error;
    }
    Object* errorPrototype()
    {
        return m_errorPrototype;
    }
    FunctionObject* referenceError()
    {
        return m_referenceError;
    }
    Object* referenceErrorPrototype()
    {
        return m_referenceErrorPrototype;
    }
    FunctionObject* typeError()
    {
        return m_typeError;
    }
    Object* typeErrorPrototype()
    {
        return m_typeErrorPrototype;
    }
    FunctionObject* rangeError()
    {
        return m_rangeError;
    }
    Object* rangeErrorPrototype()
    {
        return m_rangeErrorPrototype;
    }
    FunctionObject* syntaxError()
    {
        return m_syntaxError;
    }
    Object* syntaxErrorPrototype()
    {
        return m_syntaxErrorPrototype;
    }
    FunctionObject* uriError()
    {
        return m_uriError;
    }
    Object* uriErrorPrototype()
    {
        return m_uriErrorPrototype;
    }
    FunctionObject* evalError()
    {
        return m_evalError;
    }
    Object* evalErrorPrototype()
    {
        return m_evalErrorPrototype;
    }

    FunctionObject* string()
    {
        return m_string;
    }
    Object* stringPrototype()
    {
        return m_stringPrototype;
    }
    Object* stringIteratorPrototype()
    {
        return m_stringIteratorPrototype;
    }

    FunctionObject* number()
    {
        return m_number;
    }
    Object* numberPrototype()
    {
        return m_numberPrototype;
    }

    FunctionObject* symbol()
    {
        return m_symbol;
    }
    Object* symbolPrototype()
    {
        return m_symbolPrototype;
    }

    FunctionObject* array()
    {
        return m_array;
    }
    Object* arrayPrototype()
    {
        return m_arrayPrototype;
    }
    Object* arrayIteratorPrototype()
    {
        return m_arrayIteratorPrototype;
    }

    FunctionObject* arrayPrototypeValues()
    {
        return m_arrayPrototypeValues;
    }

    FunctionObject* boolean()
    {
        return m_boolean;
    }
    Object* booleanPrototype()
    {
        return m_booleanPrototype;
    }

    FunctionObject* date()
    {
        return m_date;
    }
    Object* datePrototype()
    {
        return m_datePrototype;
    }

    Object* math()
    {
        return m_math;
    }

    GlobalRegExpFunctionObject* regexp()
    {
        return m_regexp;
    }

    Object* regexpPrototype()
    {
        return m_regexpPrototype;
    }

    FunctionObject* regexpSplitMethod()
    {
        return m_regexpSplitMethod;
    }
    FunctionObject* regexpReplaceMethod()
    {
        return m_regexpReplaceMethod;
    }
    Object* json()
    {
        return m_json;
    }

    FunctionObject* jsonStringify()
    {
        return m_jsonStringify;
    }

    FunctionObject* jsonParse()
    {
        return m_jsonParse;
    }
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    Object* intl()
    {
        return m_intl;
    }

    FunctionObject* intlCollator()
    {
        return m_intlCollator;
    }

    const Vector<String*, gc_allocator<String*>>& intlCollatorAvailableLocales();

    FunctionObject* intlDateTimeFormat()
    {
        return m_intlDateTimeFormat;
    }

    const Vector<String*, gc_allocator<String*>>& intlDateTimeFormatAvailableLocales();

    FunctionObject* intlNumberFormat()
    {
        return m_intlNumberFormat;
    }

    const Vector<String*, gc_allocator<String*>>& intlNumberFormatAvailableLocales();
#endif
    FunctionObject* promise()
    {
        return m_promise;
    }
    Object* promisePrototype()
    {
        return m_promisePrototype;
    }
    FunctionObject* proxy()
    {
        return m_proxy;
    }
    FunctionObject* arrayBuffer()
    {
        return m_arrayBuffer;
    }
    Object* arrayBufferPrototype()
    {
        return m_arrayBufferPrototype;
    }
    FunctionObject* dataView()
    {
        return m_dataView;
    }
    Object* dataViewPrototype()
    {
        return m_dataViewPrototype;
    }
    Object* typedArray()
    {
        return m_typedArray;
    }
    Object* typedArrayPrototype()
    {
        return m_typedArrayPrototype;
    }
    Object* int8Array()
    {
        return m_int8Array;
    }
    Object* int8ArrayPrototype()
    {
        return m_int8ArrayPrototype;
    }
    Object* uint8Array()
    {
        return m_uint8Array;
    }
    Object* uint8ArrayPrototype()
    {
        return m_uint8ArrayPrototype;
    }
    Object* int16Array()
    {
        return m_int16Array;
    }
    Object* int16ArrayPrototype()
    {
        return m_int16ArrayPrototype;
    }
    Object* uint16Array()
    {
        return m_uint16Array;
    }
    Object* uint16ArrayPrototype()
    {
        return m_uint16ArrayPrototype;
    }
    Object* int32Array()
    {
        return m_int32Array;
    }
    Object* int32ArrayPrototype()
    {
        return m_int32ArrayPrototype;
    }
    Object* uint32Array()
    {
        return m_uint32Array;
    }
    Object* uint32ArrayPrototype()
    {
        return m_uint32ArrayPrototype;
    }
    Object* uint8ClampedArray()
    {
        return m_uint8ClampedArray;
    }
    Object* uint8ClampedArrayPrototype()
    {
        return m_uint8ClampedArrayPrototype;
    }
    Object* float32Array()
    {
        return m_float32Array;
    }
    Object* float32ArrayPrototype()
    {
        return m_float32ArrayPrototype;
    }
    Object* float64Array()
    {
        return m_float64Array;
    }
    Object* float64ArrayPrototype()
    {
        return m_float64ArrayPrototype;
    }

    FunctionObject* map()
    {
        return m_map;
    }

    Object* mapPrototype()
    {
        return m_mapPrototype;
    }

    Object* mapIteratorPrototype()
    {
        return m_mapIteratorPrototype;
    }

    FunctionObject* set()
    {
        return m_set;
    }

    Object* setPrototype()
    {
        return m_setPrototype;
    }

    Object* setIteratorPrototype()
    {
        return m_setIteratorPrototype;
    }

    FunctionObject* weakMap()
    {
        return m_weakMap;
    }

    Object* weakMapPrototype()
    {
        return m_weakMapPrototype;
    }

    FunctionObject* weakSet()
    {
        return m_weakSet;
    }

    Object* weakSetPrototype()
    {
        return m_weakSetPrototype;
    }

    FunctionObject* generatorFunction()
    {
        return m_generatorFunction;
    }

    FunctionObject* generator()
    {
        return m_generator;
    }

    Object* generatorPrototype()
    {
        return m_generatorPrototype;
    }

    FunctionObject* asyncFunction()
    {
        return m_asyncFunction;
    }

    Object* asyncPrototype()
    {
        return m_asyncFunctionPrototype;
    }

    FunctionObject* eval()
    {
        return m_eval;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-13.2.3
    // 13.2.3 The [[ThrowTypeError]] Function Object
    FunctionObject* throwTypeError()
    {
        ASSERT(m_throwTypeError);
        return m_throwTypeError;
    }

    JSGetterSetter* throwerGetterSetterData()
    {
        ASSERT(m_throwerGetterSetterData);
        return m_throwerGetterSetterData;
    }

    StringObject* stringProxyObject()
    {
        return m_stringProxyObject;
    }

    NumberObject* numberProxyObject()
    {
        return m_numberProxyObject;
    }

    BooleanObject* booleanProxyObject()
    {
        return m_booleanProxyObject;
    }

    SymbolObject* symbolProxyObject()
    {
        return m_symbolProxyObject;
    }

    virtual bool isInlineCacheable()
    {
        return false;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new[](size_t size) = delete;

private:
    Context* m_context;

    FunctionObject* m_object;
    Object* m_objectPrototype;
    FunctionObject* m_objectPrototypeToString;
    FunctionObject* m_objectCreate;
    FunctionObject* m_objectFreeze;

    FunctionObject* m_function;
    FunctionObject* m_functionPrototype;

    Object* m_iteratorPrototype;

    FunctionObject* m_error;
    Object* m_errorPrototype;
    FunctionObject* m_referenceError;
    Object* m_referenceErrorPrototype;
    FunctionObject* m_typeError;
    Object* m_typeErrorPrototype;
    FunctionObject* m_rangeError;
    Object* m_rangeErrorPrototype;
    FunctionObject* m_syntaxError;
    Object* m_syntaxErrorPrototype;
    FunctionObject* m_uriError;
    Object* m_uriErrorPrototype;
    FunctionObject* m_evalError;
    Object* m_evalErrorPrototype;

    FunctionObject* m_string;
    Object* m_stringPrototype;
    Object* m_stringIteratorPrototype;

    FunctionObject* m_number;
    Object* m_numberPrototype;

    FunctionObject* m_symbol;
    Object* m_symbolPrototype;

    FunctionObject* m_array;
    Object* m_arrayPrototype;
    Object* m_arrayIteratorPrototype;
    // https://www.ecma-international.org/ecma-262/6.0/#sec-well-known-intrinsic-objects
    // Well-Known Intrinsic Objects : %ArrayProto_values%
    // The initial value of the values data property of %ArrayPrototype%
    FunctionObject* m_arrayPrototypeValues;

    FunctionObject* m_boolean;
    Object* m_booleanPrototype;

    FunctionObject* m_date;
    Object* m_datePrototype;

    GlobalRegExpFunctionObject* m_regexp;
    Object* m_regexpPrototype;
    FunctionObject* m_regexpSplitMethod;
    FunctionObject* m_regexpReplaceMethod;

    Object* m_math;

    FunctionObject* m_eval;

    FunctionObject* m_throwTypeError;
    JSGetterSetter* m_throwerGetterSetterData;

    StringObject* m_stringProxyObject;
    NumberObject* m_numberProxyObject;
    BooleanObject* m_booleanProxyObject;
    SymbolObject* m_symbolProxyObject;

    Object* m_json;
    FunctionObject* m_jsonStringify;
    FunctionObject* m_jsonParse;
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    Object* m_intl;
    FunctionObject* m_intlCollator;
    Vector<String*, gc_allocator<String*>> m_intlCollatorAvailableLocales;
    FunctionObject* m_intlDateTimeFormat;
    Vector<String*, gc_allocator<String*>> m_intlDateTimeFormatAvailableLocales;
    FunctionObject* m_intlNumberFormat;
    Vector<String*, gc_allocator<String*>> m_intlNumberFormatAvailableLocales;
#endif

    FunctionObject* m_promise;
    Object* m_promisePrototype;

    FunctionObject* m_proxy;

    Object* m_reflect;

    FunctionObject* m_arrayBuffer;
    Object* m_arrayBufferPrototype;
    FunctionObject* m_dataView;
    Object* m_dataViewPrototype;
    FunctionObject* m_typedArray;
    Object* m_typedArrayPrototype;
    FunctionObject* m_int8Array;
    Object* m_int8ArrayPrototype;
    FunctionObject* m_uint8Array;
    Object* m_uint8ArrayPrototype;
    FunctionObject* m_uint8ClampedArray;
    Object* m_uint8ClampedArrayPrototype;
    FunctionObject* m_int16Array;
    Object* m_int16ArrayPrototype;
    FunctionObject* m_uint16Array;
    Object* m_uint16ArrayPrototype;
    FunctionObject* m_int32Array;
    Object* m_int32ArrayPrototype;
    FunctionObject* m_uint32Array;
    Object* m_uint32ArrayPrototype;
    FunctionObject* m_float32Array;
    Object* m_float32ArrayPrototype;
    FunctionObject* m_float64Array;
    Object* m_float64ArrayPrototype;

    FunctionObject* m_map;
    Object* m_mapPrototype;
    Object* m_mapIteratorPrototype;
    FunctionObject* m_set;
    Object* m_setPrototype;
    Object* m_setIteratorPrototype;
    FunctionObject* m_weakMap;
    Object* m_weakMapPrototype;
    FunctionObject* m_weakSet;
    Object* m_weakSetPrototype;

    FunctionObject* m_generatorFunction; // %GeneratorFunction%
    FunctionObject* m_generator; // %Generator%
    Object* m_generatorPrototype; // %GeneratorPrototype%

    FunctionObject* m_asyncFunction; // %AsyncFunction%
    Object* m_asyncFunctionPrototype; // %AsyncFunctionPrototype%
};
}

#endif
