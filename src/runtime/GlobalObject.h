/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/FunctionObject.h"
#include "runtime/Object.h"

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


class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;
    friend class GlobalEnvironmentRecord;
    friend class IdentifierNode;

    GlobalObject(ExecutionState& state)
        : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, false)
        , m_context(state.context())
    {
        m_objectPrototype = Object::createBuiltinObjectPrototype(state);
        m_objectPrototype->markThisObjectDontNeedStructureTransitionTable(state);
        setPrototype(state, m_objectPrototype);

        m_structure = m_structure->convertToWithFastAccess(state);
        m_throwTypeError = nullptr;
        m_throwerGetterSetterData = nullptr;

        // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
        giveInternalClassProperty("global");
    }

    virtual bool isGlobalObject() const
    {
        return true;
    }

    void installBuiltins(ExecutionState& state)
    {
        installFunction(state);
        installObject(state);
        installError(state);
        installString(state);
        installNumber(state);
        installBoolean(state);
        installArray(state);
        installMath(state);
        installDate(state);
        installRegExp(state);
        installJSON(state);
#if ESCARGOT_ENABLE_PROMISE
        installPromise(state);
#endif
#if ESCARGOT_ENABLE_TYPEDARRAY
        installDataView(state);
        installTypedArray(state);
#endif
        installOthers(state);
    }

    void installFunction(ExecutionState& state);
    void installObject(ExecutionState& state);
    void installError(ExecutionState& state);
    void installString(ExecutionState& state);
    void installNumber(ExecutionState& state);
    void installBoolean(ExecutionState& state);
    void installArray(ExecutionState& state);
    void installMath(ExecutionState& state);
    void installDate(ExecutionState& state);
    void installRegExp(ExecutionState& state);
    void installJSON(ExecutionState& state);
#if ESCARGOT_ENABLE_PROMISE
    void installPromise(ExecutionState& state);
#endif
#if ESCARGOT_ENABLE_TYPEDARRAY
    void installDataView(ExecutionState& state);
    void installTypedArray(ExecutionState& state);
    template <typename TA, int elementSize>
    FunctionObject* installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction);
#endif
    void installOthers(ExecutionState& state);

    Value eval(ExecutionState& state, const Value& arg);
    Value evalLocal(ExecutionState& state, const Value& arg, Value thisValue, InterpretedCodeBlock* parentCodeBlock);

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

    FunctionObject* number()
    {
        return m_number;
    }
    Object* numberPrototype()
    {
        return m_numberPrototype;
    }

    FunctionObject* array()
    {
        return m_array;
    }
    Object* arrayPrototype()
    {
        return m_arrayPrototype;
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

    FunctionObject* regexp()
    {
        return m_regexp;
    }

    Object* regexpPrototype()
    {
        return m_regexpPrototype;
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

#if ESCARGOT_ENABLE_PROMISE
    FunctionObject* promise()
    {
        return m_promise;
    }
    Object* promisePrototype()
    {
        return m_promisePrototype;
    }
#endif

#if ESCARGOT_ENABLE_TYPEDARRAY
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
#endif

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

    virtual bool isInlineCacheable()
    {
        return false;
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new[](size_t size) = delete;

protected:
    Context* m_context;

    FunctionObject* m_object;
    Object* m_objectPrototype;
    FunctionObject* m_objectPrototypeToString;

    FunctionObject* m_function;
    FunctionObject* m_functionPrototype;

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

    FunctionObject* m_number;
    Object* m_numberPrototype;

    FunctionObject* m_array;
    Object* m_arrayPrototype;

    FunctionObject* m_boolean;
    Object* m_booleanPrototype;

    FunctionObject* m_date;
    Object* m_datePrototype;

    FunctionObject* m_regexp;
    Object* m_regexpPrototype;

    Object* m_math;

    FunctionObject* m_eval;

    FunctionObject* m_throwTypeError;
    JSGetterSetter* m_throwerGetterSetterData;

    StringObject* m_stringProxyObject;
    NumberObject* m_numberProxyObject;

    Object* m_json;
    FunctionObject* m_jsonStringify;
    FunctionObject* m_jsonParse;

#if ESCARGOT_ENABLE_PROMISE
    FunctionObject* m_promise;
    Object* m_promisePrototype;
#endif

#if ESCARGOT_ENABLE_TYPEDARRAY
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
#endif
};
}

#endif
