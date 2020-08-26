/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __ESCARGOT_PUBLIC__
#define __ESCARGOT_PUBLIC__

#if !defined(ESCARGOT_EXPORT)
#if defined(_MSC_VER)
#define ESCARGOT_EXPORT __declspec(dllexport)
#else
#define ESCARGOT_EXPORT __attribute__((visibility("default")))
#endif
#endif

#include <cstdlib>
#include <cstddef>
#include <string>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#if !defined(NDEBUG) && defined(__GLIBCXX__) && !defined(_GLIBCXX_DEBUG)
#pragma message("You should define `_GLIBCXX_DEBUG` in {debug mode + libstdc++} because Escargot uses it")
#endif

namespace Escargot {

class VMInstanceRef;
class ContextRef;
class StringRef;
class RopeStringRef;
class SymbolRef;
class ValueRef;
class PointerValueRef;
class ObjectRef;
class GlobalObjectRef;
class FunctionObjectRef;
class ArrayObjectRef;
class ArrayBufferObjectRef;
class ArrayBufferViewRef;
class Int8ArrayObjectRef;
class Uint8ArrayObjectRef;
class Int16ArrayObjectRef;
class Uint16ArrayObjectRef;
class Int32ArrayObjectRef;
class Uint32ArrayObjectRef;
class Uint8ClampedArrayObjectRef;
class Float32ArrayObjectRef;
class Float64ArrayObjectRef;
class PromiseObjectRef;
class SetObjectRef;
class WeakSetObjectRef;
class MapObjectRef;
class WeakMapObjectRef;
class ErrorObjectRef;
class DateObjectRef;
class StringObjectRef;
class SymbolObjectRef;
class NumberObjectRef;
class BooleanObjectRef;
class RegExpObjectRef;
class ProxyObjectRef;
class GlobalObjectProxyObjectRef;
class PlatformRef;
class ScriptRef;
class ScriptParserRef;
class ExecutionStateRef;
class ValueVectorRef;
class JobRef;
class TemplateRef;
class ObjectTemplateRef;
class FunctionTemplateRef;

class ESCARGOT_EXPORT Globals {
public:
    static void initialize();
    static void finalize();
};

class ESCARGOT_EXPORT Memory {
public:
    static void* gcMalloc(size_t siz); // allocate memory it can hold gc-allocated pointer
    static void* gcMallocAtomic(size_t siz); // allocate memory it can not hold gc-allocated pointer like string, number
    static void* gcMallocUncollectable(size_t siz); // allocate memory it can hold gc-allocated pointer & it is never collect by gc
    static void* gcMallocAtomicUncollectable(size_t siz); // allocate memory it can not hold gc-allocated pointer & it is never collect by gc
    static void gcFree(void* ptr);
    typedef void (*GCAllocatedMemoryFinalizer)(void* self);

    // gcRegisterFinalizer
    // 1. if you want to free memory explicitly, you must remove registered finalizer
    // if there was no finalizer, you can just free memory
    // ex) void* gcPointer;
    //     gcRegisterFinalizer(gcPointer, ....);
    //     ......
    //     gcRegisterFinalizer(gcPointer, nullptr);
    //     Memory::gcFree(gcPointer);
    // 2. You cannot register finalizer to escargot's gc allocated memory eg) ObjectRef
    static void gcRegisterFinalizer(void* ptr, GCAllocatedMemoryFinalizer callback);

    static void gc();

    static size_t heapSize(); // Return the number of bytes in the heap.  Excludes bdwgc private data structures. Excludes the unmapped memory
    static size_t totalSize(); // Return the total number of bytes allocated in this process

    typedef void (*OnGCEventListener)();
    static void setGCEventListener(OnGCEventListener l);
    // NOTE bdwgc(c/c++ gc library escargot use) allocate at least N/GC_free_space_divisor bytes between collections
    // (Allocated memory by GC x 2) / (Frequency parameter value)
    // Increasing this value may use less space but there is more collection event
    static void setGCFrequency(size_t value = 1);
};

// NOTE only {stack, kinds of PersistentHolders} are root set. if you store the data you need on other space, you may lost your data
template <typename T>
class ESCARGOT_EXPORT PersistentRefHolder {
public:
    ~PersistentRefHolder()
    {
        destoryHolderSpace();
    }

    PersistentRefHolder()
    {
        m_holder = nullptr;
    }

    PersistentRefHolder(T* ptr)
    {
        initHolderSpace(ptr);
    }

    PersistentRefHolder(PersistentRefHolder<T>&& src)
    {
        m_holder = src.m_holder;
        src.m_holder = nullptr;
    }

    const PersistentRefHolder<T>& operator=(PersistentRefHolder<T>&& src)
    {
        m_holder = src.m_holder;
        src.m_holder = nullptr;

        return *this;
    }

    PersistentRefHolder(const PersistentRefHolder<T>&) = delete;
    const PersistentRefHolder<T>& operator=(const PersistentRefHolder<T>&) = delete;

    void reset(T* ptr)
    {
        if (!ptr) {
            destoryHolderSpace();
            return;
        }
        if (m_holder == nullptr) {
            initHolderSpace(ptr);
        } else {
            *m_holder = ptr;
        }
    }

    operator T*()
    {
        return *m_holder;
    }

    T* get()
    {
        return *m_holder;
    }

    T* release()
    {
        if (m_holder) {
            T* ptr = *m_holder;
            destoryHolderSpace();
            return ptr;
        }
        return nullptr;
    }

    T* operator->()
    {
        return *m_holder;
    }

private:
    void initHolderSpace(T* initialValue)
    {
        m_holder = (T**)Memory::gcMallocUncollectable(sizeof(T**));
        *m_holder = initialValue;
    }

    void destoryHolderSpace()
    {
        if (m_holder) {
            Memory::gcFree(m_holder);
        }
        m_holder = nullptr;
    }

    T** m_holder;
};

class PersistentValueRefMap {
public:
    static PersistentRefHolder<PersistentValueRefMap> create();

    // we can count 0~2^30 range
    // returns how many times rooted
    // if ValueRef has type doesn't require root ex) undefined, number, boolean, small integer value
    // this functions return 0;
    uint32_t add(ValueRef* ptr);
    // return how many times rooted after remove root
    // 0 means the value is not heap-allocated value or is removed from root list
    uint32_t remove(ValueRef* ptr);

    void clear();
};

template <typename T>
class ESCARGOT_EXPORT GCManagedVector {
public:
    GCManagedVector()
    {
        m_buffer = nullptr;
        m_size = 0;
    }

    explicit GCManagedVector(size_t size)
    {
        if (size) {
            m_buffer = (T*)Memory::gcMalloc(sizeof(T) * size);
            m_size = size;
            for (size_t i = 0; i < size; i++) {
                new (&m_buffer[i]) T();
            }
        } else {
            m_buffer = nullptr;
            m_size = 0;
        }
    }

    GCManagedVector(GCManagedVector<T>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        other.m_size = 0;
    }

    GCManagedVector(const GCManagedVector<T>& other) = delete;
    const GCManagedVector<T>& operator=(const GCManagedVector<T>& other) = delete;

    ~GCManagedVector()
    {
        if (m_buffer) {
            for (size_t i = 0; i < m_size; i++) {
                m_buffer[i].~T();
            }
            Memory::gcFree(m_buffer);
        }
    }

    size_t size() const
    {
        return m_size;
    }

    T& operator[](const size_t idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t idx) const
    {
        return m_buffer[idx];
    }

    void clear()
    {
        if (m_buffer) {
            Memory::gcFree(m_buffer);
        }
        m_size = 0;
        m_buffer = nullptr;
    }

    void* operator new(size_t size)
    {
        return Memory::gcMalloc(size);
    }
    void* operator new(size_t, void* ptr)
    {
        return ptr;
    }
    void* operator new[](size_t size) = delete;

    void operator delete(void* ptr)
    {
        Memory::gcFree(ptr);
    }
    void operator delete[](void* obj) = delete;

private:
    T* m_buffer;
    size_t m_size;
};

template <typename T>
class ESCARGOT_EXPORT OptionalRef {
public:
    OptionalRef()
        : m_value(nullptr)
    {
    }

    OptionalRef(T* value)
        : m_value(value)
    {
    }

    OptionalRef(std::nullptr_t value)
        : m_value(nullptr)
    {
    }

    OptionalRef(const OptionalRef<T>& src)
        : m_value(src.m_value)
    {
    }

    T* value()
    {
        return m_value;
    }

    const T* value() const
    {
        return m_value;
    }

    T* get()
    {
        return m_value;
    }

    const T* get() const
    {
        return m_value;
    }

    bool hasValue() const
    {
        return !!m_value;
    }

    operator bool() const
    {
        return hasValue();
    }

    T* operator->()
    {
        return m_value;
    }

    const OptionalRef<T>& operator=(const OptionalRef<T>& other)
    {
        m_value = other.m_value;
        return *this;
    }

    bool operator==(const OptionalRef<T>& other) const
    {
        return m_value == other.m_value;
    }

    bool operator!=(const OptionalRef<T>& other) const
    {
        return !this->operator==(other);
    }

    bool operator==(const T*& other) const
    {
        if (hasValue()) {
            return *value() == *other;
        }
        return false;
    }

    bool operator!=(const T*& other) const
    {
        return !operator==(other);
    }

protected:
    T* m_value;
};

// expand tuple into variadic template function's arguments
// https://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments
namespace EvaluatorUtil {
template <size_t N>
struct ApplyTupleIntoArgumentsOfVariadicTemplateFunction {
    template <typename F, typename T, typename... A>
    static inline auto apply(F&& f, T&& t, A&&... a)
        -> decltype(ApplyTupleIntoArgumentsOfVariadicTemplateFunction<N - 1>::apply(
            ::std::forward<F>(f), ::std::forward<T>(t),
            ::std::get<N - 1>(::std::forward<T>(t)), ::std::forward<A>(a)...))
    {
        return ApplyTupleIntoArgumentsOfVariadicTemplateFunction<N - 1>::apply(::std::forward<F>(f), ::std::forward<T>(t),
                                                                               ::std::get<N - 1>(::std::forward<T>(t)), ::std::forward<A>(a)...);
    }
};

template <>
struct ApplyTupleIntoArgumentsOfVariadicTemplateFunction<0> {
    template <typename F, typename T, typename... A>
    static inline auto apply(F&& f, T&&, A&&... a)
        -> decltype(::std::forward<F>(f)(::std::forward<A>(a)...))
    {
        return ::std::forward<F>(f)(::std::forward<A>(a)...);
    }
};

template <typename F, typename T>
inline auto applyTupleIntoArgumentsOfVariadicTemplateFunction(F&& f, T&& t)
    -> decltype(ApplyTupleIntoArgumentsOfVariadicTemplateFunction< ::std::tuple_size<
                    typename ::std::decay<T>::type>::value>::apply(::std::forward<F>(f), ::std::forward<T>(t)))
{
    return ApplyTupleIntoArgumentsOfVariadicTemplateFunction< ::std::tuple_size<
        typename ::std::decay<T>::type>::value>::apply(::std::forward<F>(f), ::std::forward<T>(t));
}
}

class ESCARGOT_EXPORT Evaluator {
public:
    struct LOC {
        size_t line;
        size_t column;
        size_t index;

        LOC(size_t line, size_t column, size_t index)
            : line(line)
            , column(column)
            , index(index)
        {
        }
    };

    struct StackTraceData {
        StringRef* src;
        StringRef* sourceCode;
        LOC loc;
        StringRef* functionName;
        bool isFunction;
        bool isConstructor;
        bool isAssociatedWithJavaScriptCode;
        bool isEval;
        StackTraceData();
    };

    struct EvaluatorResult {
        EvaluatorResult();
        bool isSuccessful() const
        {
            return !error.hasValue();
        }

        StringRef* resultOrErrorToString(ContextRef* ctx) const;

        ValueRef* result;
        OptionalRef<ValueRef> error;
        GCManagedVector<StackTraceData> stackTraceData;
    };

    template <typename... Args, typename F>
    static EvaluatorResult execute(ContextRef* ctx, F&& closure, Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef * state, Args...);
        return executeImpl(ctx, Closure(closure), args...);
    }

    static EvaluatorResult executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData), void* data);
    static EvaluatorResult executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData, void* passedData2), void* data, void* data2);

private:
    template <typename... Args>
    static EvaluatorResult executeImpl(ContextRef* ctx, ValueRef* (*fn)(ExecutionStateRef* state, Args...), Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef * state, Args...);
        std::tuple<ExecutionStateRef*, Args...> tuple = std::tuple<ExecutionStateRef*, Args...>(nullptr, args...);

        return executeFunction(ctx, [](ExecutionStateRef* state, void* tuplePtr, void* fnPtr) -> ValueRef* {
            std::tuple<ExecutionStateRef*, Args...>* tuple = (std::tuple<ExecutionStateRef*, Args...>*)tuplePtr;
            Closure fn = (Closure)fnPtr;

            std::get<0>(*tuple) = state;

            return EvaluatorUtil::applyTupleIntoArgumentsOfVariadicTemplateFunction(fn, *tuple);
        },
                               &tuple, (void*)fn);
    }
};

// Don't save pointer of ExecutionStateRef anywhere yourself
// If you want to acquire ExecutionStateRef, you can use Evaluator::execute
class ESCARGOT_EXPORT ExecutionStateRef {
public:
    OptionalRef<FunctionObjectRef> resolveCallee(); // resolve nearest callee if exists
    GCManagedVector<FunctionObjectRef*> resolveCallstack(); // resolve list of callee
    GlobalObjectRef* resolveCallerLexicalGlobalObject(); // resolve caller's lexical global object

    void throwException(ValueRef* value);
    bool inTryStatement(); // test ExecutionStateRef in scope of try-statement directly

    GCManagedVector<Evaluator::StackTraceData> computeStackTraceData();

    ContextRef* context();

    OptionalRef<ExecutionStateRef> parent();
};

class ESCARGOT_EXPORT VMInstanceRef {
public:
    // you can to provide timezone as TZ database name like "US/Pacific".
    // if you don't provide, we try to detect system timezone.
    static PersistentRefHolder<VMInstanceRef> create(PlatformRef* platform, const char* locale = nullptr, const char* timezone = nullptr);

    typedef void (*OnVMInstanceDelete)(VMInstanceRef* instance);
    void setOnVMInstanceDelete(OnVMInstanceDelete cb);

    // this function enforce do gc,
    // remove every compiled bytecodes,
    // remove regexp cache,
    // and compress every comressible strings if we can
    void enterIdleMode();
    // force clear every caches related with context
    // you can call this function if you don't want to use every alive contexts
    void clearCachesRelatedWithContext();

    PlatformRef* platform();

    SymbolRef* toStringTagSymbol();
    SymbolRef* iteratorSymbol();
    SymbolRef* unscopablesSymbol();
    SymbolRef* hasInstanceSymbol();
    SymbolRef* isConcatSpreadableSymbol();
    SymbolRef* speciesSymbol();
    SymbolRef* toPrimitiveSymbol();
    SymbolRef* searchSymbol();
    SymbolRef* matchSymbol();
    SymbolRef* matchAllSymbol();
    SymbolRef* replaceSymbol();
    SymbolRef* splitSymbol();
    SymbolRef* asyncIteratorSymbol();

    bool hasPendingPromiseJob();
    Evaluator::EvaluatorResult executePendingPromiseJob();
};

class ESCARGOT_EXPORT ContextRef {
public:
    static PersistentRefHolder<ContextRef> create(VMInstanceRef* vmInstance);

    void clearRelatedQueuedPromiseJobs();

    VMInstanceRef* vmInstance();
    ScriptParserRef* scriptParser();

    GlobalObjectRef* globalObject();
    ObjectRef* globalObjectProxy();
    // this setter try to update `globalThis` value on GlobalObject
    void setGlobalObjectProxy(ObjectRef* newGlobalObjectProxy);

    void throwException(ValueRef* exceptionValue); // if you use this function without Evaluator, your program will crash :(

    bool initDebugger(const char* options);
    void printDebugger(StringRef* output);
    StringRef* getClientSource(StringRef** sourceName);

    typedef OptionalRef<ValueRef> (*VirtualIdentifierCallback)(ExecutionStateRef* state, ValueRef* name);
    typedef OptionalRef<ValueRef> (*SecurityPolicyCheckCallback)(ExecutionStateRef* state, bool isEval);

    // this is not compatible with ECMAScript
    // but this callback is needed for browser-implementation
    // if there is a Identifier with that value, callback should return non empty optional value
    void setVirtualIdentifierCallback(VirtualIdentifierCallback cb);
    VirtualIdentifierCallback virtualIdentifierCallback();

    void setSecurityPolicyCheckCallback(SecurityPolicyCheckCallback cb);
};

// AtomicStringRef is never deleted by gc until VMInstance destroyed
// client doesn't need to store this ref in Persistent storage
class ESCARGOT_EXPORT AtomicStringRef {
public:
    static AtomicStringRef* create(ContextRef* c, const char* src, size_t length); // from ASCII string
    template <size_t N>
    static AtomicStringRef* create(ContextRef* c, const char (&str)[N])
    {
        return create(c, str, N - 1);
    }
    static AtomicStringRef* create(ContextRef* c, StringRef* src);
    static AtomicStringRef* emptyAtomicString();
    StringRef* string();

    bool equals(AtomicStringRef* ref) const
    {
        return this == ref;
    }
};

// only large integer, double, PointerValueRef are stored in heap
// other types are represented by pointer, but doesn't use heap
// client must save heap-allocated values on the space where gc can find from root space(stack, VMInstance, PersistentHolders) if don't want to delete by gc
class ESCARGOT_EXPORT ValueRef {
public:
    static ValueRef* create(bool);
    static ValueRef* create(int);
    static ValueRef* create(unsigned);
    static ValueRef* create(float);
    static ValueRef* create(double);
    static ValueRef* create(long);
    static ValueRef* create(unsigned long);
    static ValueRef* create(long long);
    static ValueRef* create(unsigned long long);
    static ValueRef* create(ValueRef* src)
    {
        return src;
    }
    static ValueRef* createNull();
    static ValueRef* createUndefined();

    bool isStoreInHeap();
    bool isBoolean();
    bool isNumber();
    bool isNull();
    bool isUndefined();
    bool isInt32();
    bool isUInt32();
    bool isDouble();
    bool isTrue();
    bool isFalse();
    bool isString();
    bool isSymbol();
    bool isPointerValue();
    bool isCallable(); // can ValueRef::call
    bool isConstructible(); // can ValueRef::construct
    bool isUndefinedOrNull()
    {
        return isUndefined() || isNull();
    }
    bool isObject();
    bool isFunctionObject();
    bool isArrayObject();
    bool isArrayPrototypeObject();
    bool isStringObject();
    bool isSymbolObject();
    bool isNumberObject();
    bool isBooleanObject();
    bool isRegExpObject();
    bool isDateObject();
    bool isGlobalObject();
    bool isErrorObject();
    bool isArrayBufferObject();
    bool isArrayBufferView();
    bool isInt8ArrayObject();
    bool isUint8ArrayObject();
    bool isInt16ArrayObject();
    bool isUint16ArrayObject();
    bool isInt32ArrayObject();
    bool isUint32ArrayObject();
    bool isUint8ClampedArrayObject();
    bool isFloat32ArrayObject();
    bool isFloat64ArrayObject();
    bool isTypedArrayObject();
    bool isTypedArrayPrototypeObject();
    bool isDataViewObject();
    bool isPromiseObject();
    bool isProxyObject();
    bool isArgumentsObject();
    bool isGeneratorFunctionObject();
    bool isGeneratorObject();
    bool isSetObject();
    bool isWeakSetObject();
    bool isSetIteratorObject();
    bool isMapObject();
    bool isWeakMapObject();
    bool isMapIteratorObject();
    bool isGlobalObjectProxyObject();

    bool toBoolean(ExecutionStateRef* state);
    double toNumber(ExecutionStateRef* state);
    double toInteger(ExecutionStateRef* state);
    double toLength(ExecutionStateRef* state);
    int32_t toInt32(ExecutionStateRef* state);
    uint32_t toUint32(ExecutionStateRef* state);
    StringRef* toString(ExecutionStateRef* state);
    // we never throw exception in this function but returns "Error while converting to string, but do not throw an exception" string
    StringRef* toStringWithoutException(ContextRef* ctx);
    ObjectRef* toObject(ExecutionStateRef* state);

    enum : uint64_t { InvalidIndexValue = std::numeric_limits<uint64_t>::max() };
    typedef uint64_t ValueIndex;
    ValueIndex toIndex(ExecutionStateRef* state);

    enum : uint32_t { InvalidArrayIndexValue = std::numeric_limits<uint32_t>::max() };
    uint32_t toArrayIndex(ExecutionStateRef* state);

    bool asBoolean();
    double asNumber();
    int32_t asInt32();
    uint32_t asUint32();
    PointerValueRef* asPointerValue();
    StringRef* asString();
    SymbolRef* asSymbol();
    ObjectRef* asObject();
    FunctionObjectRef* asFunctionObject();
    ArrayObjectRef* asArrayObject();
    StringObjectRef* asStringObject();
    SymbolObjectRef* asSymbolObject();
    NumberObjectRef* asNumberObject();
    BooleanObjectRef* asBooleanObject();
    RegExpObjectRef* asRegExpObject();
    DateObjectRef* asDateObject();
    GlobalObjectRef* asGlobalObject();
    ErrorObjectRef* asErrorObject();
    ArrayBufferObjectRef* asArrayBufferObject();
    ArrayBufferViewRef* asArrayBufferView();
    Int8ArrayObjectRef* asInt8ArrayObject();
    Uint8ArrayObjectRef* asUint8ArrayObject();
    Int16ArrayObjectRef* asInt16ArrayObject();
    Uint16ArrayObjectRef* asUint16ArrayObject();
    Int32ArrayObjectRef* asInt32ArrayObject();
    Uint32ArrayObjectRef* asUint32ArrayObject();
    Uint8ClampedArrayObjectRef* asUint8ClampedArrayObject();
    Float32ArrayObjectRef* asFloat32ArrayObject();
    Float64ArrayObjectRef* asFloat64ArrayObject();
    PromiseObjectRef* asPromiseObject();
    ProxyObjectRef* asProxyObject();
    SetObjectRef* asSetObject();
    WeakSetObjectRef* asWeakSetObject();
    MapObjectRef* asMapObject();
    WeakMapObjectRef* asWeakMapObject();
    GlobalObjectProxyObjectRef* asGlobalObjectProxyObject();

    bool abstractEqualsTo(ExecutionStateRef* state, const ValueRef* other) const; // ==
    bool equalsTo(ExecutionStateRef* state, const ValueRef* other) const; // ===
    bool instanceOf(ExecutionStateRef* state, const ValueRef* other) const;
    ValueRef* call(ExecutionStateRef* state, ValueRef* receiver, const size_t argc, ValueRef** argv);
    ValueRef* construct(ExecutionStateRef* state, const size_t argc, ValueRef** argv); // same with new expression in js
};

class ESCARGOT_EXPORT ValueVectorRef {
public:
    static ValueVectorRef* create(size_t size = 0);

    size_t size();
    void pushBack(ValueRef* val);
    void insert(size_t pos, ValueRef* val);
    void erase(size_t pos);
    void erase(size_t start, size_t end);
    ValueRef* at(const size_t idx);
    void set(const size_t idx, ValueRef* newValue);
    void resize(size_t newSize);
};

// `double` value is not presented in PointerValue, but it is stored in heap
class ESCARGOT_EXPORT PointerValueRef : public ValueRef {
public:
};

class ESCARGOT_EXPORT StringRef : public PointerValueRef {
public:
    template <size_t N>
    static StringRef* createFromASCII(const char (&str)[N])
    {
        return createFromASCII(str, N - 1);
    }
    static StringRef* createFromASCII(const char* s, size_t len);
    template <size_t N>
    static StringRef* createFromUTF8(const char (&str)[N])
    {
        return createFromUTF8(str, N - 1);
    }
    static StringRef* createFromUTF8(const char* s, size_t len);
    static StringRef* createFromUTF16(const char16_t* s, size_t len);
    static StringRef* createFromLatin1(const unsigned char* s, size_t len);

    static StringRef* createExternalFromASCII(const char* s, size_t len);
    static StringRef* createExternalFromLatin1(const unsigned char* s, size_t len);
    static StringRef* createExternalFromUTF16(const char16_t* s, size_t len);

    // you can use these functions only if you enabled string compression
    static bool isCompressibleStringEnabled();
    static StringRef* createFromUTF8ToCompressibleString(VMInstanceRef* instance, const char* s, size_t len);
    static StringRef* createFromUTF16ToCompressibleString(VMInstanceRef* instance, const char16_t* s, size_t len);
    static StringRef* createFromASCIIToCompressibleString(VMInstanceRef* instance, const char* s, size_t len);
    static StringRef* createFromLatin1ToCompressibleString(VMInstanceRef* instance, const unsigned char* s, size_t len);
    static void* allocateStringDataBufferForCompressibleString(size_t byteLength);
    static void deallocateStringDataBufferForCompressibleString(void* ptr, size_t byteLength);
    static StringRef* createFromAlreadyAllocatedBufferToCompressibleString(VMInstanceRef* instance, void* buffer, size_t stringLen, bool is8Bit /* is ASCII or Latin1 */);

    // you can use these functions only if you enabled reloadable string
    static bool isReloadableStringEnabled();
    static StringRef* createReloadableString(VMInstanceRef* instance,
                                             bool is8BitString, size_t len, void* callbackData,
                                             void* (*loadCallback)(void* callbackData), // you should returns string buffer
                                             void (*unloadCallback)(void* memoryPtr, void* callbackData)); // you should free memoryPtr

    static StringRef* emptyString();

    char16_t charAt(size_t idx);
    size_t length();
    bool has8BitContent();

    bool hasExternalMemory();
    bool isCompressibleString();
    bool isReloadableString();

    bool isRopeString();
    RopeStringRef* asRopeString();

    bool equals(StringRef* src);
    bool equalsWithASCIIString(const char* buf, size_t len);

    StringRef* substring(size_t from, size_t to);

    enum WriteOptions {
        NoOptions = 0,
        ReplaceInvalidUtf8 = 1 << 3
    };

    std::string toStdUTF8String(int options = NoOptions);

    // don't store this sturct or string buffer
    // this is only for temporary access
    struct StringBufferAccessDataRef {
        bool has8BitContent;
        size_t length;
        const void* buffer;

        // A type to hold a single Latin-1 character.
        typedef unsigned char LChar;

        char16_t uncheckedCharAtFor8Bit(size_t idx) const
        {
            return ((LChar*)buffer)[idx];
        }

        char16_t uncheckedCharAtFor16Bit(size_t idx) const
        {
            return ((char16_t*)buffer)[idx];
        }

        char16_t charAt(size_t idx) const
        {
            if (has8BitContent) {
                return ((LChar*)buffer)[idx];
            } else {
                return ((char16_t*)buffer)[idx];
            }
        }
    };

    StringBufferAccessDataRef stringBufferAccessData();
};

class ESCARGOT_EXPORT RopeStringRef : public StringRef {
public:
    bool wasFlattened();

    // you can get left, right value when RopeString is not flattened
    OptionalRef<StringRef> left();
    OptionalRef<StringRef> right();
};

class ESCARGOT_EXPORT SymbolRef : public PointerValueRef {
public:
    static SymbolRef* create(StringRef* desc);
    static SymbolRef* fromGlobalSymbolRegistry(VMInstanceRef* context, StringRef* desc); // this is same with Symbol.for
    StringRef* description();
    StringRef* symbolDescriptiveString();
};

class ObjectPropertyDescriptorRef {
public:
    ObjectPropertyDescriptorRef(ValueRef* value);
    ObjectPropertyDescriptorRef(ValueRef* value, bool writable);
    ObjectPropertyDescriptorRef(ValueRef* getter, ValueRef* setter);

    ObjectPropertyDescriptorRef(const ObjectPropertyDescriptorRef& src);
    const ObjectPropertyDescriptorRef& operator=(const ObjectPropertyDescriptorRef& src);
    ~ObjectPropertyDescriptorRef();

    ValueRef* value();
    bool hasValue();

    ValueRef* getter();
    bool hasGetter();
    ValueRef* setter();
    bool hasSetter();

    bool isEnumerable();
    void setEnumerable(bool enumerable);
    bool hasEnumerable();

    bool isConfigurable();
    void setConfigurable(bool configurable);
    bool hasConfigurable();

    bool isWritable();
    bool hasWritable();

    static void* operator new(size_t) = delete;
    static void* operator new[](size_t) = delete;

private:
    friend class ObjectWithNamedPropertyHandler;
    ObjectPropertyDescriptorRef(void* src);

    void* m_privateData;
};

struct ESCARGOT_EXPORT ExposableObjectGetOwnPropertyCallbackResult {
    ExposableObjectGetOwnPropertyCallbackResult()
        : m_value()
        , m_isWritable(false)
        , m_isEnumerable(false)
        , m_isConfigurable(false)
    {
    }

    ExposableObjectGetOwnPropertyCallbackResult(ValueRef* value, bool isWritable, bool isEnumerable, bool isConfigurable)
        : m_value(value)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
    {
    }

    OptionalRef<ValueRef> m_value;
    bool m_isWritable;
    bool m_isEnumerable;
    bool m_isConfigurable;
};

struct ESCARGOT_EXPORT ExposableObjectEnumerationCallbackResult {
    ExposableObjectEnumerationCallbackResult(ValueRef* name = ValueRef::createUndefined(), bool isWritable = true, bool isEnumerable = true, bool isConfigurable = true)
        : m_name(name)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
    {
    }

    ValueRef* m_name;
    bool m_isWritable;
    bool m_isEnumerable;
    bool m_isConfigurable;
};

typedef ExposableObjectGetOwnPropertyCallbackResult (*ExposableObjectGetOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName);
typedef bool (*ExposableObjectDefineOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName, ValueRef* value);
typedef GCManagedVector<ExposableObjectEnumerationCallbackResult> ExposableObjectEnumerationCallbackResultVector;
typedef ExposableObjectEnumerationCallbackResultVector (*ExposableObjectEnumerationCallback)(ExecutionStateRef* state, ObjectRef* self);
typedef bool (*ExposableObjectDeleteOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* propertyName);

class ESCARGOT_EXPORT ObjectRef : public PointerValueRef {
public:
    static ObjectRef* create(ExecutionStateRef* state);
    // can not redefine or delete virtual property
    // virtual property does not follow every rule of ECMAScript
    static ObjectRef* createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback);

    ValueRef* get(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnPropertyDescriptor(ExecutionStateRef* state, ValueRef* propertyName);

    bool set(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value);

    bool has(ExecutionStateRef* state, ValueRef* propertyName);

    enum PresentAttribute {
        NotPresent = 0,
        WritablePresent = 1 << 1,
        EnumerablePresent = 1 << 2,
        ConfigurablePresent = 1 << 3,
        NonWritablePresent = 1 << 4,
        NonEnumerablePresent = 1 << 5,
        NonConfigurablePresent = 1 << 6,
        AllPresent = WritablePresent | EnumerablePresent | ConfigurablePresent
    };

    class DataPropertyDescriptor {
    public:
        DataPropertyDescriptor(ValueRef* value, PresentAttribute attr)
            : m_value(value)
            , m_attribute(attr)
        {
        }

        ValueRef* m_value;
        PresentAttribute m_attribute;
    };

    class AccessorPropertyDescriptor {
    public:
        // only undefined, empty optional value, function are allowed to getter, setter parameter
        // empty optional value means unspecified state
        // undefined means truely undefined -> ex) { get: undefined }
        AccessorPropertyDescriptor(ValueRef* getter, OptionalRef<ValueRef> setter, PresentAttribute attr)
            : m_getter(getter)
            , m_setter(setter)
            , m_attribute(attr)
        {
        }

        ValueRef* m_getter;
        OptionalRef<ValueRef> m_setter;
        PresentAttribute m_attribute;
    };

    bool defineOwnProperty(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* desc);
    bool defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, const DataPropertyDescriptor& desc);
    bool defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value, bool isWritable, bool isEnumerable, bool isConfigurable);
    bool defineAccessorProperty(ExecutionStateRef* state, ValueRef* propertyName, const AccessorPropertyDescriptor& desc);

    struct NativeDataAccessorPropertyData;
    typedef ValueRef* (*NativeDataAccessorPropertyGetter)(ExecutionStateRef* state, ObjectRef* self, NativeDataAccessorPropertyData* data);
    typedef bool (*NativeDataAccessorPropertySetter)(ExecutionStateRef* state, ObjectRef* self, NativeDataAccessorPropertyData* data, ValueRef* setterInputData);
    // client extend this struct to give data for getter, setter if needs
    // this struct must allocated in gc-heap
    // only setter can be null
    struct ESCARGOT_EXPORT NativeDataAccessorPropertyData {
        bool m_isWritable : 1;
        bool m_isEnumerable : 1;
        bool m_isConfigurable : 1;
        NativeDataAccessorPropertyGetter m_getter;
        NativeDataAccessorPropertySetter m_setter;

        NativeDataAccessorPropertyData(bool isWritable, bool isEnumerable, bool isConfigurable,
                                       NativeDataAccessorPropertyGetter getter, NativeDataAccessorPropertySetter setter)
            : m_isWritable(isWritable)
            , m_isEnumerable(isEnumerable)
            , m_isConfigurable(isConfigurable)
            , m_getter(getter)
            , m_setter(setter)
        {
        }

        void* operator new(size_t size);
        void* operator new[](size_t size) = delete;
    };
    // this function differ with defineDataPropety and defineAccessorPropety.
    // !hasOwnProperty(state, P) is needed for success
    bool defineNativeDataAccessorProperty(ExecutionStateRef* state, ValueRef* P, NativeDataAccessorPropertyData* data);

    bool deleteOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    bool hasOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);

    bool deleteProperty(ExecutionStateRef* state, ValueRef* propertyName);
    bool hasProperty(ExecutionStateRef* state, ValueRef* propertyName);

    ValueRef* getPrototype(ExecutionStateRef* state);
    OptionalRef<ObjectRef> getPrototypeObject(ExecutionStateRef* state); // if __proto__ is not object(undefined or null), this function returns nullptr instead of orginal value.
    bool setPrototype(ExecutionStateRef* state, ValueRef* value);

    OptionalRef<ContextRef> creationContext();

    ValueVectorRef* ownPropertyKeys(ExecutionStateRef* state);

    void enumerateObjectOwnProperies(ExecutionStateRef* state, const std::function<bool(ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>& cb);

    bool isExtensible(ExecutionStateRef* state);
    bool preventExtensions(ExecutionStateRef* state);

    void* extraData();
    void setExtraData(void* e);

    void removeFromHiddenClassChain();

    // DEPRECATED! this function will be removed
    void removeFromHiddenClassChain(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT GlobalObjectRef : public ObjectRef {
public:
    FunctionObjectRef* object();
    ObjectRef* objectPrototype();
    FunctionObjectRef* objectPrototypeToString();
    FunctionObjectRef* function();
    FunctionObjectRef* functionPrototype();
    FunctionObjectRef* error();
    ObjectRef* errorPrototype();
    FunctionObjectRef* referenceError();
    ObjectRef* referenceErrorPrototype();
    FunctionObjectRef* typeError();
    ObjectRef* typeErrorPrototype();
    FunctionObjectRef* rangeError();
    ObjectRef* rangeErrorPrototype();
    FunctionObjectRef* syntaxError();
    ObjectRef* syntaxErrorPrototype();
    FunctionObjectRef* uriError();
    ObjectRef* uriErrorPrototype();
    FunctionObjectRef* evalError();
    ObjectRef* evalErrorPrototype();
    FunctionObjectRef* string();
    ObjectRef* stringPrototype();
    FunctionObjectRef* number();
    ObjectRef* numberPrototype();
    FunctionObjectRef* array();
    ObjectRef* arrayPrototype();
    FunctionObjectRef* boolean();
    ObjectRef* booleanPrototype();
    FunctionObjectRef* date();
    ObjectRef* datePrototype();
    ObjectRef* math();
    FunctionObjectRef* regexp();
    ObjectRef* regexpPrototype();
    ObjectRef* json();
    FunctionObjectRef* jsonStringify();
    FunctionObjectRef* jsonParse();

    FunctionObjectRef* promise();
    ObjectRef* promisePrototype();

    FunctionObjectRef* arrayBuffer();
    ObjectRef* arrayBufferPrototype();
    FunctionObjectRef* dataView();
    ObjectRef* dataViewPrototype();
    ObjectRef* int8Array();
    ObjectRef* int8ArrayPrototype();
    ObjectRef* uint8Array();
    ObjectRef* uint8ArrayPrototype();
    ObjectRef* int16Array();
    ObjectRef* int16ArrayPrototype();
    ObjectRef* uint16Array();
    ObjectRef* uint16ArrayPrototype();
    ObjectRef* int32Array();
    ObjectRef* int32ArrayPrototype();
    ObjectRef* uint32Array();
    ObjectRef* uint32ArrayPrototype();
    ObjectRef* uint8ClampedArray();
    ObjectRef* uint8ClampedArrayPrototype();
    ObjectRef* float32Array();
    ObjectRef* float32ArrayPrototype();
    ObjectRef* float64Array();
    ObjectRef* float64ArrayPrototype();
};

class ESCARGOT_EXPORT GlobalObjectProxyObjectRef : public ObjectRef {
public:
    // if there is security error, you can throw error in this callback
    enum AccessOperationType {
        Read, // [[Get]], [[GetOwnProperty]]..
        Write // [[Set]], [[DefineOwnProperty]]..
    };
    typedef void (*SecurityCheckCallback)(ExecutionStateRef* state, GlobalObjectProxyObjectRef* proxy, GlobalObjectRef* targetGlobalObject, AccessOperationType operationType, OptionalRef<AtomicStringRef> nonIndexedStringPropertyNameIfExists);

    static GlobalObjectProxyObjectRef* create(ExecutionStateRef* state, GlobalObjectRef* target, SecurityCheckCallback callback);

    GlobalObjectRef* target();
    void setTarget(GlobalObjectRef* target);
};

class ESCARGOT_EXPORT FunctionObjectRef : public ObjectRef {
public:
    // if newTarget is present, that means constructor call
    // in constructor call, function must return newly created object && thisValue is always undefined
    typedef ValueRef* (*NativeFunctionPointer)(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, bool isConstructorCall);

    enum BuiltinFunctionSlot : size_t {
        PublicFunctionIndex = 0,
    };

    struct NativeFunctionInfo {
        bool m_isStrict;
        bool m_isConstructor;
        AtomicStringRef* m_name;
        NativeFunctionPointer m_nativeFunction;
        size_t m_argumentCount;

        NativeFunctionInfo(AtomicStringRef* name, NativeFunctionPointer fn, size_t argc, bool isStrict = true, bool isConstructor = true)
            : m_isStrict(isStrict)
            , m_isConstructor(isConstructor)
            , m_name(name)
            , m_nativeFunction(fn)
            , m_argumentCount(argc)
        {
        }
    };

    static FunctionObjectRef* create(ExecutionStateRef* state, NativeFunctionInfo info);
    static FunctionObjectRef* createBuiltinFunction(ExecutionStateRef* state, NativeFunctionInfo info); // protoype of builtin function is non-writable

    // get prototype property of constructible function(not [[prototype]])
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    ValueRef* getFunctionPrototype(ExecutionStateRef* state);

    // set prototype property constructible function(not [[prototype]])
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    bool setFunctionPrototype(ExecutionStateRef* state, ValueRef* v);

    bool isConstructor();
    void markFunctionNeedsSlowVirtualIdentifierOperation();
};

class ESCARGOT_EXPORT IteratorObjectRef : public ObjectRef {
public:
    static IteratorObjectRef* create(ExecutionStateRef* state);
    ValueRef* next(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT ArrayObjectRef : public ObjectRef {
public:
    static ArrayObjectRef* create(ExecutionStateRef* state);
    static ArrayObjectRef* create(ExecutionStateRef* state, ValueVectorRef* source);
    IteratorObjectRef* values(ExecutionStateRef* state);
    IteratorObjectRef* keys(ExecutionStateRef* state);
    IteratorObjectRef* entries(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT ErrorObjectRef : public ObjectRef {
public:
    enum Code {
        None,
        ReferenceError,
        TypeError,
        SyntaxError,
        RangeError,
        URIError,
        EvalError
    };
    static ErrorObjectRef* create(ExecutionStateRef* state, ErrorObjectRef::Code code, StringRef* errorMessage);
};

class ESCARGOT_EXPORT ReferenceErrorObjectRef : public ErrorObjectRef {
public:
    static ReferenceErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT TypeErrorObjectRef : public ErrorObjectRef {
public:
    static TypeErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT SyntaxErrorObjectRef : public ErrorObjectRef {
public:
    static SyntaxErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT RangeErrorObjectRef : public ErrorObjectRef {
public:
    static RangeErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT URIErrorObjectRef : public ErrorObjectRef {
public:
    static URIErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT EvalErrorObjectRef : public ErrorObjectRef {
public:
    static EvalErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
};

class ESCARGOT_EXPORT DateObjectRef : public ObjectRef {
public:
    static DateObjectRef* create(ExecutionStateRef* state);
    static int64_t currentTime();
    void setTimeValue(ExecutionStateRef* state, ValueRef* str);
    void setTimeValue(int64_t value);
    StringRef* toUTCString(ExecutionStateRef* state);
    double primitiveValue();
};

class ESCARGOT_EXPORT StringObjectRef : public ObjectRef {
public:
    static StringObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    StringRef* primitiveValue();
};

class ESCARGOT_EXPORT SymbolObjectRef : public ObjectRef {
public:
    static SymbolObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, SymbolRef* value);
    SymbolRef* primitiveValue();
};

class ESCARGOT_EXPORT NumberObjectRef : public ObjectRef {
public:
    static NumberObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    double primitiveValue();
};

class ESCARGOT_EXPORT BooleanObjectRef : public ObjectRef {
public:
    static BooleanObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, ValueRef* value);
    bool primitiveValue();
};

class ESCARGOT_EXPORT RegExpObjectRef : public ObjectRef {
public:
    enum RegExpObjectOption {
        None = 0 << 0,
        Global = 1 << 0,
        IgnoreCase = 1 << 1,
        MultiLine = 1 << 2,
        Sticky = 1 << 3,
        Unicode = 1 << 4,
        DotAll = 1 << 5,
    };

    static RegExpObjectRef* create(ExecutionStateRef* state, ValueRef* source, ValueRef* option);
    StringRef* source();
    RegExpObjectOption option();
};

class ESCARGOT_EXPORT ArrayBufferObjectRef : public ObjectRef {
public:
    static ArrayBufferObjectRef* create(ExecutionStateRef* state);
    void allocateBuffer(ExecutionStateRef* state, size_t bytelength);
    void attachBuffer(ExecutionStateRef* state, void* buffer, size_t bytelength);
    void detachArrayBuffer(ExecutionStateRef* state);
    uint8_t* rawBuffer();
    size_t byteLength();
    bool isDetachedBuffer();
};

class ESCARGOT_EXPORT ArrayBufferViewRef : public ObjectRef {
public:
    ArrayBufferObjectRef* buffer();
    void setBuffer(ArrayBufferObjectRef* bo, size_t byteOffset, size_t byteLength, size_t arrayLength);
    void setBuffer(ArrayBufferObjectRef* bo, size_t byteOffset, size_t byteLength);
    uint8_t* rawBuffer();
    size_t byteLength();
    size_t byteOffset();
    size_t arrayLength();
};

class ESCARGOT_EXPORT Int8ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Int8ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Uint8ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Uint8ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Int16ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Int16ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Uint16ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Uint16ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Uint32ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Uint32ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Int32ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Int32ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Uint8ClampedArrayObjectRef : public ArrayBufferViewRef {
public:
    static Uint8ClampedArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Float32ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Float32ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT Float64ArrayObjectRef : public ArrayBufferViewRef {
public:
    static Float64ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT PromiseObjectRef : public ObjectRef {
public:
    static PromiseObjectRef* create(ExecutionStateRef* state);

    enum PromiseState {
        Pending,
        FulFilled,
        Rejected
    };
    PromiseState state();
    ValueRef* promiseResult();

    ObjectRef* then(ExecutionStateRef* state, ValueRef* handler);
    ObjectRef* catchOperation(ExecutionStateRef* state, ValueRef* handler);
    ObjectRef* then(ExecutionStateRef* state, ValueRef* onFulfilled, ValueRef* onRejected);
    void fulfill(ExecutionStateRef* state, ValueRef* value);
    void reject(ExecutionStateRef* state, ValueRef* reason);
};

class ESCARGOT_EXPORT ProxyObjectRef : public ObjectRef {
public:
    static ProxyObjectRef* create(ExecutionStateRef* state, ObjectRef* target, ObjectRef* handler);
    ObjectRef* target();
    ObjectRef* handler();
    bool isRevoked();
    void revoke();
};

class ESCARGOT_EXPORT SetObjectRef : public ObjectRef {
public:
    static SetObjectRef* create(ExecutionStateRef* state);
    void add(ExecutionStateRef* state, ValueRef* key);
    void clear(ExecutionStateRef* state);
    bool deleteOperation(ExecutionStateRef* state, ValueRef* key);
    bool has(ExecutionStateRef* state, ValueRef* key);
    size_t size(ExecutionStateRef* state);
    IteratorObjectRef* values(ExecutionStateRef* state);
    IteratorObjectRef* keys(ExecutionStateRef* state);
    IteratorObjectRef* entries(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT WeakSetObjectRef : public ObjectRef {
public:
    static WeakSetObjectRef* create(ExecutionStateRef* state);
    void add(ExecutionStateRef* state, ObjectRef* key);
    bool deleteOperation(ExecutionStateRef* state, ObjectRef* key);
    bool has(ExecutionStateRef* state, ObjectRef* key);
};

class ESCARGOT_EXPORT MapObjectRef : public ObjectRef {
public:
    static MapObjectRef* create(ExecutionStateRef* state);
    void clear(ExecutionStateRef* state);
    bool deleteOperation(ExecutionStateRef* state, ValueRef* key);
    ValueRef* get(ExecutionStateRef* state, ValueRef* key);
    bool has(ExecutionStateRef* state, ValueRef* key);
    void set(ExecutionStateRef* state, ValueRef* key, ValueRef* value);
    size_t size(ExecutionStateRef* state);
    IteratorObjectRef* values(ExecutionStateRef* state);
    IteratorObjectRef* keys(ExecutionStateRef* state);
    IteratorObjectRef* entries(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT WeakMapObjectRef : public ObjectRef {
public:
    static WeakMapObjectRef* create(ExecutionStateRef* state);
    bool deleteOperation(ExecutionStateRef* state, ObjectRef* key);
    ValueRef* get(ExecutionStateRef* state, ObjectRef* key);
    bool has(ExecutionStateRef* state, ObjectRef* key);
    void set(ExecutionStateRef* state, ObjectRef* key, ValueRef* value);
};

class ESCARGOT_EXPORT TemplatePropertyNameRef {
public:
    TemplatePropertyNameRef(StringRef* name = StringRef::emptyString())
        : m_ptr(name)
    {
    }
    TemplatePropertyNameRef(SymbolRef* name)
        : m_ptr(name)
    {
    }

    PointerValueRef* value() const
    {
        return m_ptr;
    }

private:
    PointerValueRef* m_ptr;
};

// don't modify template after instantiate object
// it is not intented operation
class ESCARGOT_EXPORT TemplateRef {
public:
    void set(const TemplatePropertyNameRef& name, ValueRef* data, bool isWritable, bool isEnumerable, bool isConfigurable);
    void set(const TemplatePropertyNameRef& name, TemplateRef* data, bool isWritable, bool isEnumerable, bool isConfigurable);
    void setAccessorProperty(const TemplatePropertyNameRef& name, OptionalRef<FunctionTemplateRef> getter, OptionalRef<FunctionTemplateRef> setter, bool isEnumerable, bool isConfigurable);
    void setNativeDataAccessorProperty(const TemplatePropertyNameRef& name, ObjectRef::NativeDataAccessorPropertyGetter getter, ObjectRef::NativeDataAccessorPropertySetter setter,
                                       bool isWritable, bool isEnumerable, bool isConfigurable);
    void setNativeDataAccessorProperty(const TemplatePropertyNameRef& name, ObjectRef::NativeDataAccessorPropertyData* data);

    bool has(const TemplatePropertyNameRef& name);
    // return true if removed
    bool remove(const TemplatePropertyNameRef& name);

    ObjectRef* instantiate(ContextRef* ctx);
    bool didInstantiate();

    bool isObjectTemplate();
    bool isFunctionTemplate();

    void setInstanceExtraData(void* ptr);
    void* instanceExtraData();
};

typedef OptionalRef<ValueRef> (*TemplateNamedPropertyHandlerGetterCallback)(ExecutionStateRef* state, ObjectRef* self, void* data, const TemplatePropertyNameRef& propertyName);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*TemplateNamedPropertyHandlerSetterCallback)(ExecutionStateRef* state, ObjectRef* self, void* data, const TemplatePropertyNameRef& propertyName, ValueRef* value);
enum TemplatePropertyAttribute {
    TemplatePropertyAttributeNotExist = 1 << 0,
    TemplatePropertyAttributeExist = 1 << 1,
    TemplatePropertyAttributeWritable = 1 << 2,
    TemplatePropertyAttributeEnumerable = 1 << 3,
    TemplatePropertyAttributeConfigurable = 1 << 4,
};
typedef TemplatePropertyAttribute (*TemplateNamedPropertyHandlerQueryCallback)(ExecutionStateRef* state, ObjectRef* self, void* data, const TemplatePropertyNameRef& propertyName);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*TemplateNamedPropertyHandlerDeleteCallback)(ExecutionStateRef* state, ObjectRef* self, void* data, const TemplatePropertyNameRef& propertyName);
typedef GCManagedVector<TemplatePropertyNameRef> TemplateNamedPropertyHandlerEnumerationCallbackResultVector;
typedef TemplateNamedPropertyHandlerEnumerationCallbackResultVector (*TemplateNamedPropertyHandlerEnumerationCallback)(ExecutionStateRef* state, ObjectRef* self, void* data);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*TemplateNamedPropertyHandlerDefineOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, void* data, const TemplatePropertyNameRef& propertyName, const ObjectPropertyDescriptorRef& desc);
typedef OptionalRef<ObjectRef> (*TemplateNamedPropertyHandlerGetPropertyDescriptorCallback)(ExecutionStateRef* state, ObjectRef* self, void* data);

struct ESCARGOT_EXPORT ObjectTemplateNamedPropertyHandlerData {
    TemplateNamedPropertyHandlerGetterCallback getter;
    TemplateNamedPropertyHandlerSetterCallback setter;
    TemplateNamedPropertyHandlerQueryCallback query;
    TemplateNamedPropertyHandlerDeleteCallback deleter;
    TemplateNamedPropertyHandlerEnumerationCallback enumerator;
    TemplateNamedPropertyHandlerDefineOwnPropertyCallback definer;
    TemplateNamedPropertyHandlerGetPropertyDescriptorCallback descriptor;
    void* data;

    ObjectTemplateNamedPropertyHandlerData(
        TemplateNamedPropertyHandlerGetterCallback getter = nullptr,
        TemplateNamedPropertyHandlerSetterCallback setter = nullptr,
        TemplateNamedPropertyHandlerQueryCallback query = nullptr,
        TemplateNamedPropertyHandlerDeleteCallback deleter = nullptr,
        TemplateNamedPropertyHandlerEnumerationCallback enumerator = nullptr,
        TemplateNamedPropertyHandlerDefineOwnPropertyCallback definer = nullptr,
        TemplateNamedPropertyHandlerGetPropertyDescriptorCallback descriptor = nullptr,
        void* data = nullptr)
        : getter(getter)
        , setter(setter)
        , query(query)
        , deleter(deleter)
        , enumerator(enumerator)
        , definer(definer)
        , descriptor(descriptor)
        , data(data)
    {
    }
};

class ESCARGOT_EXPORT ObjectTemplateRef : public TemplateRef {
public:
    static ObjectTemplateRef* create();
    void setNamedPropertyHandler(const ObjectTemplateNamedPropertyHandlerData& data);
    void removeNamedPropertyHandler();
};

// FunctionTemplateRef returns the unique function instance in context.
class ESCARGOT_EXPORT FunctionTemplateRef : public TemplateRef {
public:
    // in constructor call, thisValue is default consturcted object
    typedef ValueRef* (*NativeFunctionPointer)(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget);

    static FunctionTemplateRef* create(AtomicStringRef* name, size_t argumentCount, bool isStrict, bool isConstructor,
                                       FunctionTemplateRef::NativeFunctionPointer fn);

    void updateCallbackFunction(FunctionTemplateRef::NativeFunctionPointer fn);

    ObjectTemplateRef* prototypeTemplate();
    // ObjectTemplate for new'ed instance of this functionTemplate
    ObjectTemplateRef* instanceTemplate();

    void inherit(OptionalRef<FunctionTemplateRef> parent);
    OptionalRef<FunctionTemplateRef> parent();
};

class ESCARGOT_EXPORT ScriptParserRef {
public:
    struct InitializeScriptResult {
        bool isSuccessful()
        {
            return script.hasValue();
        }

        OptionalRef<ScriptRef> script;
        StringRef* parseErrorMessage;
        ErrorObjectRef::Code parseErrorCode;

        InitializeScriptResult();
        ScriptRef* fetchScriptThrowsExceptionIfParseError(ExecutionStateRef* state);
    };

    InitializeScriptResult initializeScript(StringRef* source, StringRef* srcName, bool isModule = false);
};

class ESCARGOT_EXPORT ScriptRef {
public:
    bool isModule();
    bool isExecuted();
    StringRef* src();
    StringRef* sourceCode();
    ValueRef* execute(ExecutionStateRef* state);

    // only module can use these functions
    size_t moduleRequestsLength();
    StringRef* moduleRequest(size_t i);
    ObjectRef* moduleNamespace(ExecutionStateRef* state);
    bool wasThereErrorOnModuleEvaluation();
    ValueRef* moduleEvaluationError();
};

class ESCARGOT_EXPORT PlatformRef {
public:
    virtual ~PlatformRef() {}
    // ArrayBuffer
    // client must returns zero-filled memory
    virtual void* onArrayBufferObjectDataBufferMalloc(ContextRef* whereObjectMade, ArrayBufferObjectRef* obj, size_t sizeInByte)
    {
        return calloc(sizeInByte, 1);
    }
    virtual void onArrayBufferObjectDataBufferFree(ContextRef* whereObjectMade, ArrayBufferObjectRef* obj, void* buffer)
    {
        return free(buffer);
    }

    // Promise
    // If you want to use promise on Escargot, you should call VMInstanceRef::executePendingPromiseJob after event. see Shell.cpp
    virtual void didPromiseJobEnqueued(ContextRef* relatedContext, PromiseObjectRef* obj) = 0;

    // Module
    // client needs cache module map<absolute_module_path, ScriptRef*>
    struct LoadModuleResult {
        LoadModuleResult(ScriptRef* result);
        LoadModuleResult(ErrorObjectRef::Code errorCode, StringRef* errorMessage);

        OptionalRef<ScriptRef> script;
        StringRef* errorMessage;
        ErrorObjectRef::Code errorCode;
    };
    virtual LoadModuleResult onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc) = 0;
    virtual void didLoadModule(ContextRef* relatedContext, OptionalRef<ScriptRef> whereRequestFrom, ScriptRef* loadedModule) = 0;

    // Dynamic Import
    virtual void hostImportModuleDynamically(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, PromiseObjectRef* promise) = 0;
};

} // namespace Escargot

#endif
