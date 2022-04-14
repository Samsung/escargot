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

#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/Object.h"
#include "runtime/FunctionObject.h"
#include "runtime/PrototypeObject.h"

namespace Escargot {

class FunctionObject;

#define RESOLVE_THIS_BINDING_TO_OBJECT(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                              \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                                        \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                           \
    Object* NAME = thisValue.toObject(state);

#define RESOLVE_THIS_BINDING_TO_STRING(NAME, OBJ, BUILT_IN_METHOD)                                                                                                                                                                              \
    if (thisValue.isUndefinedOrNull()) {                                                                                                                                                                                                        \
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().OBJ.string(), true, state.context()->staticStrings().BUILT_IN_METHOD.string(), ErrorObject::Messages::GlobalObject_ThisUndefinedOrNull); \
    }                                                                                                                                                                                                                                           \
    String* NAME = thisValue.toString(state);


#define GLOBALOBJECT_BUILTIN_ARRAYBUFFER(F, objName) \
    F(arrayBuffer, FunctionObject, objName)          \
    F(arrayBufferPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_ARRAY(F, objName) \
    F(array, FunctionObject, objName)          \
    F(arrayPrototype, Object, objName)         \
    F(arrayIteratorPrototype, Object, objName) \
    F(arrayPrototypeValues, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_ASYNCFROMSYNCITERATOR(F, objName) \
    F(asyncFromSyncIteratorPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_ASYNCFUNCTION(F, objName) \
    F(asyncFunction, FunctionObject, objName)          \
    F(asyncFunctionPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_ASYNCGENERATOR(F, objName) \
    F(asyncGenerator, Object, objName)                  \
    F(asyncGeneratorPrototype, Object, objName)         \
    F(asyncGeneratorFunction, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_BOOLEAN(F, objName) \
    F(boolean, FunctionObject, objName)          \
    F(booleanPrototype, Object, objName)         \
    F(booleanProxyObject, BooleanObject, objName)
#define GLOBALOBJECT_BUILTIN_DATAVIEW(F, objName) \
    F(dataView, FunctionObject, objName)          \
    F(dataViewPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_DATE(F, objName) \
    F(date, FunctionObject, objName)          \
    F(datePrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_ERROR(F, objName)  \
    F(error, FunctionObject, objName)           \
    F(errorPrototype, Object, objName)          \
    F(referenceError, FunctionObject, objName)  \
    F(referenceErrorPrototype, Object, objName) \
    F(typeError, FunctionObject, objName)       \
    F(typeErrorPrototype, Object, objName)      \
    F(rangeError, FunctionObject, objName)      \
    F(rangeErrorPrototype, Object, objName)     \
    F(syntaxError, FunctionObject, objName)     \
    F(syntaxErrorPrototype, Object, objName)    \
    F(uriError, FunctionObject, objName)        \
    F(uriErrorPrototype, Object, objName)       \
    F(evalError, FunctionObject, objName)       \
    F(evalErrorPrototype, Object, objName)      \
    F(aggregateError, FunctionObject, objName)  \
    F(aggregateErrorPrototype, Object, objName) \
    F(throwTypeError, FunctionObject, objName)  \
    F(throwerGetterSetterData, JSGetterSetter, objName)
#define GLOBALOBJECT_BUILTIN_FUNCTION(F, objName) \
    F(function, FunctionObject, objName)          \
    F(functionPrototype, FunctionObject, objName) \
    F(functionApply, FunctionObject, objName)     \
    F(callerAndArgumentsGetterSetter, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_GENERATOR(F, objName) \
    F(generatorFunction, FunctionObject, objName)  \
    F(generator, FunctionObject, objName)          \
    F(generatorPrototype, Object, objName)
//INTL
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#if defined(ENABLE_INTL_DISPLAYNAMES)
#define GLOBALOBJECT_BUILTIN_INTL_DISPLAYNAMES(F, objName) \
    F(intlDisplayNames, FunctionObject, objName)           \
    F(intlDisplayNamesPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL_DISPLAYNAMES(F, objName)
#endif
#if defined(ENABLE_INTL_NUMBERFORMAT)
#define GLOBALOBJECT_BUILTIN_INTL_NUMBERFORMAT(F, objName) \
    F(intlNumberFormat, FunctionObject, objName)           \
    F(intlNumberFormatPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL_NUMBERFORMAT(F, objName)
#endif
#if defined(ENABLE_INTL_PLURALRULES)
#define GLOBALOBJECT_BUILTIN_INTL_PLURALRULES(F, objName) \
    F(intlPluralRules, FunctionObject, objName)           \
    F(intlPluralRulesPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL_PLURALRULES(F, objName)
#endif
#if defined(ENABLE_INTL_RELATIVETIMEFORMAT)
#define GLOBALOBJECT_BUILTIN_INTL_RELATIVETIMEFORMAT(F, objName) \
    F(intlRelativeTimeFormat, FunctionObject, objName)           \
    F(intlRelativeTimeFormatPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL_RELATIVETIMEFORMAT(F, objName)
#endif
#if defined(ENABLE_INTL_LISTFORMAT)
#define GLOBALOBJECT_BUILTIN_INTL_LISTFORMAT(F, objName) \
    F(intlListFormat, FunctionObject, objName)           \
    F(intlListFormatPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL_LISTFORMAT(F, objName)
#endif

#define GLOBALOBJECT_BUILTIN_INTL(F, objName)                \
    F(intl, Object, objName)                                 \
    F(intlCollator, FunctionObject, objName)                 \
    F(intlDateTimeFormat, FunctionObject, objName)           \
    F(intlDateTimeFormatPrototype, Object, objName)          \
    F(intlLocale, FunctionObject, objName)                   \
    F(intlLocalePrototype, Object, objName)                  \
    GLOBALOBJECT_BUILTIN_INTL_DISPLAYNAMES(F, objName)       \
    GLOBALOBJECT_BUILTIN_INTL_NUMBERFORMAT(F, objName)       \
    GLOBALOBJECT_BUILTIN_INTL_PLURALRULES(F, objName)        \
    GLOBALOBJECT_BUILTIN_INTL_RELATIVETIMEFORMAT(F, objName) \
    GLOBALOBJECT_BUILTIN_INTL_LISTFORMAT(F, objName)
#else
#define GLOBALOBJECT_BUILTIN_INTL(F, objName)
#endif
#define GLOBALOBJECT_BUILTIN_JSON(F, objName) \
    F(json, Object, objName)                  \
    F(jsonStringify, FunctionObject, objName) \
    F(jsonParse, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_MAP(F, objName) \
    F(map, FunctionObject, objName)          \
    F(mapPrototype, Object, objName)         \
    F(mapIteratorPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_MATH(F, objName) \
    F(math, Object, objName)
#define GLOBALOBJECT_BUILTIN_NUMBER(F, objName) \
    F(number, FunctionObject, objName)          \
    F(numberPrototype, Object, objName)         \
    F(numberProxyObject, NumberObject, objName)
#define GLOBALOBJECT_BUILTIN_OBJECT(F, objName) \
    F(object, FunctionObject, objName)          \
    F(objectCreate, FunctionObject, objName)    \
    F(objectFreeze, FunctionObject, objName)    \
    F(objectPrototype, Object, objName)         \
    F(objectPrototypeToString, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_OTHERS(F, objName) \
    F(eval, FunctionObject, objName)            \
    F(parseInt, FunctionObject, objName)        \
    F(parseFloat, FunctionObject, objName)      \
    F(arrayToString, FunctionObject, objName)   \
    F(asyncIteratorPrototype, Object, objName)  \
    F(iteratorPrototype, Object, objName)       \
    F(genericIteratorPrototype, Object, objName)

#define GLOBALOBJECT_BUILTIN_PROMISE(F, objName) \
    F(promise, FunctionObject, objName)          \
    F(promisePrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_PROXY(F, objName) \
    F(proxy, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_REFLECT(F, objName) \
    F(reflect, Object, objName)
#define GLOBALOBJECT_BUILTIN_REGEXP(F, objName)       \
    F(regexp, FunctionObject, objName)                \
    F(regexpExecMethod, FunctionObject, objName)      \
    F(regexpPrototype, Object, objName)               \
    F(regexpReplaceMethod, FunctionObject, objName)   \
    F(regexpStringIteratorPrototype, Object, objName) \
    F(regexpSplitMethod, FunctionObject, objName)
#define GLOBALOBJECT_BUILTIN_SET(F, objName) \
    F(set, FunctionObject, objName)          \
    F(setPrototype, Object, objName)         \
    F(setIteratorPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_STRING(F, objName) \
    F(string, FunctionObject, objName)          \
    F(stringPrototype, Object, objName)         \
    F(stringIteratorPrototype, Object, objName) \
    F(stringProxyObject, StringObject, objName)
#define GLOBALOBJECT_BUILTIN_SYMBOL(F, objName) \
    F(symbol, FunctionObject, objName)          \
    F(symbolPrototype, Object, objName)         \
    F(symbolProxyObject, SymbolObject, objName)
#define GLOBALOBJECT_BUILTIN_BIGINT(F, objName) \
    F(bigInt, FunctionObject, objName)          \
    F(bigIntPrototype, Object, objName)         \
    F(bigIntProxyObject, BigIntObject, objName)
#define GLOBALOBJECT_BUILTIN_TYPEDARRAY(F, objName) \
    F(typedArray, FunctionObject, objName)          \
    F(typedArrayPrototype, Object, objName)         \
    F(int8Array, FunctionObject, objName)           \
    F(int8ArrayPrototype, Object, objName)          \
    F(uint8Array, FunctionObject, objName)          \
    F(uint8ArrayPrototype, Object, objName)         \
    F(uint8ClampedArray, FunctionObject, objName)   \
    F(uint8ClampedArrayPrototype, Object, objName)  \
    F(int16Array, FunctionObject, objName)          \
    F(int16ArrayPrototype, Object, objName)         \
    F(uint16Array, FunctionObject, objName)         \
    F(uint16ArrayPrototype, Object, objName)        \
    F(int32Array, FunctionObject, objName)          \
    F(int32ArrayPrototype, Object, objName)         \
    F(uint32Array, FunctionObject, objName)         \
    F(uint32ArrayPrototype, Object, objName)        \
    F(float32Array, FunctionObject, objName)        \
    F(float32ArrayPrototype, Object, objName)       \
    F(float64Array, FunctionObject, objName)        \
    F(float64ArrayPrototype, Object, objName)       \
    F(bigInt64Array, FunctionObject, objName)       \
    F(bigInt64ArrayPrototype, Object, objName)      \
    F(bigUint64Array, FunctionObject, objName)      \
    F(bigUint64ArrayPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_WEAKMAP(F, objName) \
    F(weakMap, FunctionObject, objName)          \
    F(weakMapPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_WEAKSET(F, objName) \
    F(weakSet, FunctionObject, objName)          \
    F(weakSetPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_WEAKREF(F, objName) \
    F(weakRef, FunctionObject, objName)          \
    F(weakRefPrototype, Object, objName)
#define GLOBALOBJECT_BUILTIN_FINALIZATIONREGISTRY(F, objName) \
    F(finalizationRegistry, FunctionObject, objName)          \
    F(finalizationRegistryPrototype, Object, objName)

#if defined(ENABLE_THREADING)
#define GLOBALOBJECT_BUILTIN_ATOMICS(F, objName) \
    F(atomics, Object, objName)
#define GLOBALOBJECT_BUILTIN_SHAREDARRAYBUFFER(F, objName) \
    F(sharedArrayBuffer, FunctionObject, objName)          \
    F(sharedArrayBufferPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_ATOMICS(F, objName)
#define GLOBALOBJECT_BUILTIN_SHAREDARRAYBUFFER(F, objName)
#endif

//WebAssembly
#if defined(ENABLE_WASM)
#define GLOBALOBJECT_BUILTIN_WASM(F, objName)     \
    F(wasm, Object, objName)                      \
    F(wasmModulePrototype, Object, objName)       \
    F(wasmInstancePrototype, Object, objName)     \
    F(wasmMemoryPrototype, Object, objName)       \
    F(wasmTablePrototype, Object, objName)        \
    F(wasmGlobalPrototype, Object, objName)       \
    F(wasmCompileErrorPrototype, Object, objName) \
    F(wasmLinkErrorPrototype, Object, objName)    \
    F(wasmRuntimeErrorPrototype, Object, objName)
#else
#define GLOBALOBJECT_BUILTIN_WASM(F, objName)
#endif


#define GLOBALOBJECT_BUILTIN_OBJECT_LIST(F, ARG)         \
    F(ARRAYBUFFER, ArrayBuffer, ARG)                     \
    F(ARRAY, Array, ARG)                                 \
    F(ASYNCFROMSYNCITERATOR, AsyncFromSyncIterator, ARG) \
    F(ASYNCFUNCTION, AsyncFunction, ARG)                 \
    F(ASYNCGENERATOR, AsyncGenerator, ARG)               \
    F(ATOMICS, Atomics, ARG)                             \
    F(BOOLEAN, Boolean, ARG)                             \
    F(DATAVIEW, DataView, ARG)                           \
    F(DATE, Date, ARG)                                   \
    F(ERROR, Error, ARG)                                 \
    F(FUNCTION, Function, ARG)                           \
    F(GENERATOR, Generator, ARG)                         \
    F(INTL, Intl, ARG)                                   \
    F(JSON, JSON, ARG)                                   \
    F(MAP, Map, ARG)                                     \
    F(MATH, Math, ARG)                                   \
    F(NUMBER, Number, ARG)                               \
    F(OBJECT, Object, ARG)                               \
    F(OTHERS, Others, ARG)                               \
    F(PROMISE, Promise, ARG)                             \
    F(PROXY, Proxy, ARG)                                 \
    F(REFLECT, Reflect, ARG)                             \
    F(REGEXP, RegExp, ARG)                               \
    F(SET, Set, ARG)                                     \
    F(SHAREDARRAYBUFFER, SharedArrayBuffer, ARG)         \
    F(STRING, String, ARG)                               \
    F(SYMBOL, Symbol, ARG)                               \
    F(BIGINT, BigInt, ARG)                               \
    F(TYPEDARRAY, TypedArray, ARG)                       \
    F(WEAKMAP, WeakMap, ARG)                             \
    F(WEAKSET, WeakSet, ARG)                             \
    F(WEAKREF, WeakRef, ARG)                             \
    F(FINALIZATIONREGISTRY, FinalizationRegistry, ARG)   \
    F(WASM, WebAssembly, ARG)


#define DECLARE_BUILTIN_ALL_LIST(OBJNAME, objName, ARG) \
    GLOBALOBJECT_BUILTIN_##OBJNAME(ARG, objName)

#define GLOBALOBJECT_BUILTIN_ALL_LIST(F) \
    GLOBALOBJECT_BUILTIN_OBJECT_LIST(DECLARE_BUILTIN_ALL_LIST, F)


Value builtinSpeciesGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

class GlobalObject : public PrototypeObject {
public:
    friend class GlobalEnvironmentRecord;

    explicit GlobalObject(ExecutionState& state);

    virtual bool isGlobalObject() const override
    {
        return true;
    }

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override;

    /*
       Global builtin property getter method
       If builtin property is not yet installed (m_##builtin is null), call install#objName method to install it
       Installation is executed only once for each builtin object
    */
#define DECLARE_BUILTIN_GETTER_FUNC(builtin, TYPE, objName) \
    TYPE* builtin()                                         \
    {                                                       \
        if (UNLIKELY(!m_##builtin)) {                       \
            ExecutionState tempState(m_context);            \
            install##objName(tempState);                    \
        }                                                   \
        ASSERT(!!m_##builtin);                              \
        return m_##builtin;                                 \
    }

    GLOBALOBJECT_BUILTIN_ALL_LIST(DECLARE_BUILTIN_GETTER_FUNC)
#undef DECLARE_BUILTIN_GETTER_FUNC

    void initializeBuiltins(ExecutionState& state);

    Value eval(ExecutionState& state, const Value& arg);
    // we get isInWithOperation as parameter because this affects bytecode
    Value evalLocal(ExecutionState& state, const Value& arg, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool inWithOperation);

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new[](size_t size) = delete;

private:
    Context* m_context;

#define DECLARE_BUILTIN_MEMBER_VALUE(builtin, TYPE, objName) \
    TYPE* m_##builtin;

    GLOBALOBJECT_BUILTIN_ALL_LIST(DECLARE_BUILTIN_MEMBER_VALUE)
#undef DECLARE_BUILTIN_MEMBER_VALUE

#define DECLARE_BUILTIN_MEMBER_FUNC(OBJNAME, objName, ARG) \
    void initialize##objName(ExecutionState& state);       \
    void install##objName(ExecutionState& state);

    GLOBALOBJECT_BUILTIN_OBJECT_LIST(DECLARE_BUILTIN_MEMBER_FUNC, )
#undef DECLARE_BUILTIN_MEMBER_FUNC

    template <typename TA, int elementSize>
    FunctionObject* installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction);
};
} // namespace Escargot

#endif
