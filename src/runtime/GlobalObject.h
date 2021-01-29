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

#include "runtime/FunctionObject.h"
#include "runtime/Object.h"

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


#define GLOBALOBJECT_BUILTIN_ARRAYBUFFER(F, NAME) \
    F(arrayBuffer, FunctionObject, NAME)          \
    F(arrayBufferPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_ARRAY(F, NAME) \
    F(array, FunctionObject, NAME)          \
    F(arrayPrototype, Object, NAME)         \
    F(arrayIteratorPrototype, Object, NAME) \
    F(arrayPrototypeValues, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_ASYNCFROMSYNCITERATOR(F, NAME) \
    F(asyncFromSyncIteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_ASYNCFUNCTION(F, NAME) \
    F(asyncFunction, FunctionObject, NAME)          \
    F(asyncFunctionPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_ASYNCGENERATOR(F, NAME) \
    F(asyncGenerator, Object, NAME)                  \
    F(asyncGeneratorPrototype, Object, NAME)         \
    F(asyncGeneratorFunction, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_ASYNCITERATOR(F, NAME) \
    F(asyncIteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_BOOLEAN(F, NAME) \
    F(boolean, FunctionObject, NAME)          \
    F(booleanPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_DATAVIEW(F, NAME) \
    F(dataView, FunctionObject, NAME)          \
    F(dataViewPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_DATE(F, NAME) \
    F(date, FunctionObject, NAME)          \
    F(datePrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_ERROR(F, NAME)  \
    F(error, FunctionObject, NAME)           \
    F(errorPrototype, Object, NAME)          \
    F(referenceError, FunctionObject, NAME)  \
    F(referenceErrorPrototype, Object, NAME) \
    F(typeError, FunctionObject, NAME)       \
    F(typeErrorPrototype, Object, NAME)      \
    F(rangeError, FunctionObject, NAME)      \
    F(rangeErrorPrototype, Object, NAME)     \
    F(syntaxError, FunctionObject, NAME)     \
    F(syntaxErrorPrototype, Object, NAME)    \
    F(uriError, FunctionObject, NAME)        \
    F(uriErrorPrototype, Object, NAME)       \
    F(evalError, FunctionObject, NAME)       \
    F(evalErrorPrototype, Object, NAME)      \
    F(aggregateError, FunctionObject, NAME)  \
    F(aggregateErrorPrototype, Object, NAME) \
    F(throwTypeError, FunctionObject, NAME)  \
    F(throwerGetterSetterData, JSGetterSetter, NAME)
#define GLOBALOBJECT_BUILTIN_EVAL(F, NAME) \
    F(eval, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_FUNCTION(F, NAME) \
    F(function, FunctionObject, NAME)          \
    F(functionPrototype, FunctionObject, NAME) \
    F(functionApply, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_GENERATOR(F, NAME) \
    F(generatorFunction, FunctionObject, NAME)  \
    F(generator, FunctionObject, NAME)          \
    F(generatorPrototype, Object, NAME)
//INTL
#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#define GLOBALOBJECT_BUILTIN_INTL(F, NAME)           \
    F(intl, Object, NAME)                            \
    F(intlCollator, FunctionObject, NAME)            \
    F(intlDateTimeFormat, FunctionObject, NAME)      \
    F(intlDateTimeFormatPrototype, Object, NAME)     \
    F(intlNumberFormat, FunctionObject, NAME)        \
    F(intlNumberFormatPrototype, Object, NAME)       \
    F(intlRelativeTimeFormat, FunctionObject, NAME)  \
    F(intlRelativeTimeFormatPrototype, Object, NAME) \
    F(intlLocale, FunctionObject, NAME)              \
    F(intlLocalePrototype, Object, NAME)             \
    F(intlPluralRules, FunctionObject, NAME)         \
    F(intlPluralRulesPrototype, Object, NAME)
#else
#define GLOBALOBJECT_BUILTIN_INTL(F, NAME)
#endif
#define GLOBALOBJECT_BUILTIN_ITERATOR(F, NAME) \
    F(iteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_JSON(F, NAME) \
    F(json, Object, NAME)                  \
    F(jsonStringify, FunctionObject, NAME) \
    F(jsonParse, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_MAP(F, NAME) \
    F(map, FunctionObject, NAME)          \
    F(mapPrototype, Object, NAME)         \
    F(mapIteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_MATH(F, NAME) \
    F(math, Object, NAME)
#define GLOBALOBJECT_BUILTIN_NUMBER(F, NAME) \
    F(number, FunctionObject, NAME)          \
    F(numberPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_OBJECT(F, NAME) \
    F(object, FunctionObject, NAME)          \
    F(objectCreate, FunctionObject, NAME)    \
    F(objectFreeze, FunctionObject, NAME)    \
    F(objectPrototype, Object, NAME)         \
    F(objectPrototypeToString, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_OTHERS(F, NAME)   \
    F(stringProxyObject, StringObject, NAME)   \
    F(numberProxyObject, NumberObject, NAME)   \
    F(booleanProxyObject, BooleanObject, NAME) \
    F(symbolProxyObject, SymbolObject, NAME)   \
    F(bigIntProxyObject, BigIntObject, NAME)
#define GLOBALOBJECT_BUILTIN_PROMISE(F, NAME) \
    F(promise, FunctionObject, NAME)          \
    F(promisePrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_PROXY(F, NAME) \
    F(proxy, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_REFLECT(F, NAME) \
    F(reflect, Object, NAME)
#define GLOBALOBJECT_BUILTIN_REGEXP(F, NAME)       \
    F(regexp, FunctionObject, NAME)                \
    F(regexpExecMethod, FunctionObject, NAME)      \
    F(regexpPrototype, Object, NAME)               \
    F(regexpReplaceMethod, FunctionObject, NAME)   \
    F(regexpStringIteratorPrototype, Object, NAME) \
    F(regexpSplitMethod, FunctionObject, NAME)
#define GLOBALOBJECT_BUILTIN_SET(F, NAME) \
    F(set, FunctionObject, NAME)          \
    F(setPrototype, Object, NAME)         \
    F(setIteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_STRING(F, NAME) \
    F(string, FunctionObject, NAME)          \
    F(stringPrototype, Object, NAME)         \
    F(stringIteratorPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_SYMBOL(F, NAME) \
    F(symbol, FunctionObject, NAME)          \
    F(symbolPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_BIGINT(F, NAME) \
    F(bigInt, FunctionObject, NAME)          \
    F(bigIntPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_TYPEDARRAY(F, NAME) \
    F(typedArray, FunctionObject, NAME)          \
    F(typedArrayPrototype, Object, NAME)         \
    F(int8Array, FunctionObject, NAME)           \
    F(int8ArrayPrototype, Object, NAME)          \
    F(uint8Array, FunctionObject, NAME)          \
    F(uint8ArrayPrototype, Object, NAME)         \
    F(uint8ClampedArray, FunctionObject, NAME)   \
    F(uint8ClampedArrayPrototype, Object, NAME)  \
    F(int16Array, FunctionObject, NAME)          \
    F(int16ArrayPrototype, Object, NAME)         \
    F(uint16Array, FunctionObject, NAME)         \
    F(uint16ArrayPrototype, Object, NAME)        \
    F(int32Array, FunctionObject, NAME)          \
    F(int32ArrayPrototype, Object, NAME)         \
    F(uint32Array, FunctionObject, NAME)         \
    F(uint32ArrayPrototype, Object, NAME)        \
    F(float32Array, FunctionObject, NAME)        \
    F(float32ArrayPrototype, Object, NAME)       \
    F(float64Array, FunctionObject, NAME)        \
    F(float64ArrayPrototype, Object, NAME)       \
    F(bigInt64Array, FunctionObject, NAME)       \
    F(bigInt64ArrayPrototype, Object, NAME)      \
    F(bigUint64Array, FunctionObject, NAME)      \
    F(bigUint64ArrayPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_WEAKMAP(F, NAME) \
    F(weakMap, FunctionObject, NAME)          \
    F(weakMapPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_WEAKSET(F, NAME) \
    F(weakSet, FunctionObject, NAME)          \
    F(weakSetPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_WEAKREF(F, NAME) \
    F(weakRef, FunctionObject, NAME)          \
    F(weakRefPrototype, Object, NAME)
#define GLOBALOBJECT_BUILTIN_FINALIZATIONREGISTRY(F, NAME) \
    F(finalizationRegistry, FunctionObject, NAME)          \
    F(finalizationRegistryPrototype, Object, NAME)
//WebAssembly
#if defined(ENABLE_WASM)
#define GLOBALOBJECT_BUILTIN_WASM(F, NAME)     \
    F(wasmModulePrototype, Object, Name)       \
    F(wasmInstancePrototype, Object, Name)     \
    F(wasmMemoryPrototype, Object, Name)       \
    F(wasmTablePrototype, Object, Name)        \
    F(wasmGlobalPrototype, Object, Name)       \
    F(wasmCompileErrorPrototype, Object, NAME) \
    F(wasmLinkErrorPrototype, Object, NAME)    \
    F(wasmRuntimeErrorPrototype, Object, NAME)
#else
#define GLOBALOBJECT_BUILTIN_WASM(F, NAME)
#endif


#define GLOBALOBJECT_BUILTIN_LIST(F)                                     \
    GLOBALOBJECT_BUILTIN_ARRAYBUFFER(F, ArrayBuffer)                     \
    GLOBALOBJECT_BUILTIN_ARRAY(F, Array)                                 \
    GLOBALOBJECT_BUILTIN_ASYNCFROMSYNCITERATOR(F, AsyncFromSyncIterator) \
    GLOBALOBJECT_BUILTIN_ASYNCFUNCTION(F, AsyncFunction)                 \
    GLOBALOBJECT_BUILTIN_ASYNCGENERATOR(F, AsyncGenerator)               \
    GLOBALOBJECT_BUILTIN_ASYNCITERATOR(F, AsyncIterator)                 \
    GLOBALOBJECT_BUILTIN_BOOLEAN(F, Boolean)                             \
    GLOBALOBJECT_BUILTIN_DATAVIEW(F, DataView)                           \
    GLOBALOBJECT_BUILTIN_DATE(F, Date)                                   \
    GLOBALOBJECT_BUILTIN_ERROR(F, Error)                                 \
    GLOBALOBJECT_BUILTIN_EVAL(F, Eval)                                   \
    GLOBALOBJECT_BUILTIN_FUNCTION(F, Function)                           \
    GLOBALOBJECT_BUILTIN_GENERATOR(F, Generator)                         \
    GLOBALOBJECT_BUILTIN_INTL(F, Intl)                                   \
    GLOBALOBJECT_BUILTIN_ITERATOR(F, Iterator)                           \
    GLOBALOBJECT_BUILTIN_JSON(F, JSON)                                   \
    GLOBALOBJECT_BUILTIN_MAP(F, Map)                                     \
    GLOBALOBJECT_BUILTIN_MATH(F, Math)                                   \
    GLOBALOBJECT_BUILTIN_NUMBER(F, Number)                               \
    GLOBALOBJECT_BUILTIN_OBJECT(F, Object)                               \
    GLOBALOBJECT_BUILTIN_OTHERS(F, Others)                               \
    GLOBALOBJECT_BUILTIN_PROMISE(F, Promise)                             \
    GLOBALOBJECT_BUILTIN_PROXY(F, Proxy)                                 \
    GLOBALOBJECT_BUILTIN_REFLECT(F, Reflect)                             \
    GLOBALOBJECT_BUILTIN_REGEXP(F, RegExp)                               \
    GLOBALOBJECT_BUILTIN_SET(F, Set)                                     \
    GLOBALOBJECT_BUILTIN_STRING(F, String)                               \
    GLOBALOBJECT_BUILTIN_SYMBOL(F, Symbol)                               \
    GLOBALOBJECT_BUILTIN_BIGINT(F, BigInt)                               \
    GLOBALOBJECT_BUILTIN_TYPEDARRAY(F, TypedArray)                       \
    GLOBALOBJECT_BUILTIN_WEAKMAP(F, WeakMap)                             \
    GLOBALOBJECT_BUILTIN_WEAKSET(F, WeakSet)                             \
    GLOBALOBJECT_BUILTIN_WEAKREF(F, WeakRef)                             \
    GLOBALOBJECT_BUILTIN_FINALIZATIONREGISTRY(F, FinalizationRegistry)   \
    GLOBALOBJECT_BUILTIN_WASM(F, WebAssembly)


Value builtinSpeciesGetter(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

class GlobalObject : public Object {
public:
    friend class ByteCodeInterpreter;
    friend class GlobalEnvironmentRecord;
    friend class IdentifierNode;

    explicit GlobalObject(ExecutionState& state);

    virtual bool isGlobalObject() const override
    {
        return true;
    }

    void installBuiltins(ExecutionState& state);

    Value eval(ExecutionState& state, const Value& arg);
    // we get isInWithOperation as parameter because this affects bytecode
    Value evalLocal(ExecutionState& state, const Value& arg, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool inWithOperation);

#define DECLARE_BUILTIN_FUNC(builtin, TYPE, NAME) \
    TYPE* builtin()                               \
    {                                             \
        ASSERT(!!m_##builtin);                    \
        return m_##builtin;                       \
    }

    GLOBALOBJECT_BUILTIN_LIST(DECLARE_BUILTIN_FUNC)
#undef DECLARE_BUILTIN_FUNC

    virtual bool isInlineCacheable() override
    {
        return false;
    }

    virtual ObjectHasPropertyResult hasProperty(ExecutionState& state, const ObjectPropertyName& P) override ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;
    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE;

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new[](size_t size) = delete;

private:
    Context* m_context;

#define DECLARE_BUILTIN_VALUE(builtin, TYPE, NAME) \
    TYPE* m_##builtin;

    GLOBALOBJECT_BUILTIN_LIST(DECLARE_BUILTIN_VALUE)
#undef DECLARE_BUILTIN_VALUE

    void installFunction(ExecutionState& state);
    void installObject(ExecutionState& state);
    void installError(ExecutionState& state);
    void installSymbol(ExecutionState& state);
    void installBigInt(ExecutionState& state);
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
    void installArrayBuffer(ExecutionState& state);
    void installDataView(ExecutionState& state);
    void installTypedArray(ExecutionState& state);
    template <typename TA, int elementSize>
    FunctionObject* installTypedArray(ExecutionState& state, AtomicString taName, Object** proto, FunctionObject* typedArrayFunction);
    void installIterator(ExecutionState& state);
    void installMap(ExecutionState& state);
    void installSet(ExecutionState& state);
    void installWeakMap(ExecutionState& state);
    void installWeakRef(ExecutionState& state);
    void installWeakSet(ExecutionState& state);
    void installFinalizationRegistry(ExecutionState& state);
    void installGenerator(ExecutionState& state);
    void installAsyncFunction(ExecutionState& state);
    void installAsyncIterator(ExecutionState& state);
    void installAsyncFromSyncIterator(ExecutionState& state);
    void installAsyncGeneratorFunction(ExecutionState& state);
#if defined(ENABLE_WASM)
    void installWASM(ExecutionState& state);
#endif
    void installOthers(ExecutionState& state);
};
} // namespace Escargot

#endif
