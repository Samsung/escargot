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
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <sstream>

#if !defined(NDEBUG) && defined(__GLIBCXX__) && !defined(_GLIBCXX_DEBUG)
#pragma message("You should define `_GLIBCXX_DEBUG` in {debug mode + libstdc++} because Escargot uses it")
#endif

#define ESCARGOT_POINTERVALUE_CHILD_REF_LIST(F) \
    F(ArrayBuffer)                              \
    F(ArrayBufferObject)                        \
    F(ArrayBufferView)                          \
    F(ArrayObject)                              \
    F(BigInt)                                   \
    F(BigIntObject)                             \
    F(BooleanObject)                            \
    F(DataViewObject)                           \
    F(DateObject)                               \
    F(ErrorObject)                              \
    F(FinalizationRegistryObject)               \
    F(FunctionObject)                           \
    F(GenericIteratorObject)                    \
    F(GlobalObject)                             \
    F(GlobalObjectProxyObject)                  \
    F(IteratorObject)                           \
    F(MapObject)                                \
    F(NumberObject)                             \
    F(Object)                                   \
    F(PromiseObject)                            \
    F(ProxyObject)                              \
    F(RegExpObject)                             \
    F(SetObject)                                \
    F(SharedArrayBufferObject)                  \
    F(String)                                   \
    F(StringObject)                             \
    F(Symbol)                                   \
    F(SymbolObject)                             \
    F(WeakMapObject)                            \
    F(WeakRefObject)                            \
    F(WeakSetObject)

#define ESCARGOT_ERROR_REF_LIST(F) \
    F(AggregateErrorObject)        \
    F(EvalErrorObject)             \
    F(RangeErrorObject)            \
    F(ReferenceErrorObject)        \
    F(SyntaxErrorObject)           \
    F(TypeErrorObject)             \
    F(URIErrorObject)

#define ESCARGOT_TYPEDARRAY_REF_LIST(F) \
    F(BigInt64ArrayObject)              \
    F(BigUint64ArrayObject)             \
    F(Float32ArrayObject)               \
    F(Float64ArrayObject)               \
    F(Int16ArrayObject)                 \
    F(Int32ArrayObject)                 \
    F(Int8ArrayObject)                  \
    F(Uint16ArrayObject)                \
    F(Uint32ArrayObject)                \
    F(Uint8ArrayObject)                 \
    F(Uint8ClampedArrayObject)

#define ESCARGOT_REF_LIST(F)                \
    F(Context)                              \
    F(ExecutionState)                       \
    F(FunctionTemplate)                     \
    F(ObjectTemplate)                       \
    F(PointerValue)                         \
    F(RopeString)                           \
    F(Script)                               \
    F(ScriptParser)                         \
    F(Template)                             \
    F(VMInstance)                           \
    F(BackingStore)                         \
    ESCARGOT_POINTERVALUE_CHILD_REF_LIST(F) \
    ESCARGOT_ERROR_REF_LIST(F)              \
    ESCARGOT_TYPEDARRAY_REF_LIST(F)


namespace Escargot {

class ValueRef;
class PlatformRef;
#define DECLARE_REF_CLASS(Name) class Name##Ref;
ESCARGOT_REF_LIST(DECLARE_REF_CLASS);
#undef DECLARE_REF_CLASS

class ESCARGOT_EXPORT Globals {
public:
    // Escargot has thread-independent Globals.
    // Users should call initialize, finalize once in the main program
    static void initialize(PlatformRef* platform);
    static void finalize();

    // Globals also used for thread initialization
    // Users need to call initializeThread, finalizeThread function for each thread
    static void initializeThread();
    static void finalizeThread();

    static bool isInitialized();

    static bool supportsThreading();

    static const char* version();
    static const char* buildDate();
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
    //     gcRegisterFinalizer(gcPointer, nullptr); // this removes finalizer
    //     Memory::gcFree(gcPointer);
    static void gcRegisterFinalizer(void* ptr, GCAllocatedMemoryFinalizer callback);

    static void gcRegisterFinalizer(ObjectRef* ptr, GCAllocatedMemoryFinalizer callback);
    static void gcUnregisterFinalizer(ObjectRef* ptr, GCAllocatedMemoryFinalizer callback);

    static void gc();

    static size_t heapSize(); // Return the number of bytes in the heap.  Excludes bdwgc private data structures. Excludes the unmapped memory
    static size_t totalSize(); // Return the total number of bytes allocated in this process

    enum GCEventType {
        MARK_START,
        MARK_END,
        RECLAIM_START,
        RECLAIM_END,
    };
    // pointer `data` is not a GC object
    typedef void (*OnGCEventListener)(void* data);
    static void addGCEventListener(GCEventType type, OnGCEventListener l, void* data);
    static bool removeGCEventListener(GCEventType type, OnGCEventListener l, void* data);

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
        m_holder = (T**)Memory::gcMallocUncollectable(sizeof(T*));
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

    GCManagedVector(const GCManagedVector<T>& other)
    {
        m_buffer = nullptr;
        m_size = 0;
        copyFrom(other);
    }

    const GCManagedVector<T>& operator=(const GCManagedVector<T>& other)
    {
        copyFrom(other);
        return *this;
    }

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
    void copyFrom(const GCManagedVector<T>& other)
    {
        clear();
        m_size = other.size();
        m_buffer = (T*)Memory::gcMalloc(sizeof(T) * m_size);
        for (size_t i = 0; i < m_size; i++) {
            new (&m_buffer[i]) T(other[i]);
        }
    }
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
    -> decltype(ApplyTupleIntoArgumentsOfVariadicTemplateFunction<::std::tuple_size<
                    typename ::std::decay<T>::type>::value>::apply(::std::forward<F>(f), ::std::forward<T>(t)))
{
    return ApplyTupleIntoArgumentsOfVariadicTemplateFunction<::std::tuple_size<
        typename ::std::decay<T>::type>::value>::apply(::std::forward<F>(f), ::std::forward<T>(t));
}
} // namespace EvaluatorUtil

class ESCARGOT_EXPORT Evaluator {
public:
    struct ESCARGOT_EXPORT LOC {
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

    struct ESCARGOT_EXPORT StackTraceData {
        StringRef* srcName;
        StringRef* sourceCode;
        LOC loc;
        StringRef* functionName;
        OptionalRef<FunctionObjectRef> callee;
        bool isFunction;
        bool isConstructor;
        bool isAssociatedWithJavaScriptCode;
        bool isEval;
        StackTraceData();
    };

    struct ESCARGOT_EXPORT EvaluatorResult {
        EvaluatorResult();
        EvaluatorResult(const EvaluatorResult& src);
        const EvaluatorResult& operator=(EvaluatorResult& src);
        EvaluatorResult(EvaluatorResult&& src);

        bool isSuccessful() const
        {
            return !error.hasValue();
        }

        StringRef* resultOrErrorToString(ContextRef* ctx) const;

        ValueRef* result;
        OptionalRef<ValueRef> error;
        GCManagedVector<StackTraceData> stackTrace;
    };

    template <typename... Args, typename F>
    static EvaluatorResult execute(ContextRef* ctx, F&& closure, Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef * state, Args...);
        return executeImpl(ctx, Closure(closure), args...);
    }

    template <typename... Args, typename F>
    static EvaluatorResult execute(ExecutionStateRef* parent, F&& closure, Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef * state, Args...);
        return executeImpl(parent, Closure(closure), args...);
    }

    static EvaluatorResult executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData), void* data);
    static EvaluatorResult executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData, void* passedData2), void* data, void* data2);
    static EvaluatorResult executeFunction(ExecutionStateRef* parent, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData, void* passedData2), void* data, void* data2);

private:
    template <typename P, typename... Args>
    static EvaluatorResult executeImpl(P* p, ValueRef* (*fn)(ExecutionStateRef* state, Args...), Args... args)
    {
        typedef ValueRef* (*Closure)(ExecutionStateRef * state, Args...);
        std::tuple<ExecutionStateRef*, Args...> tuple = std::tuple<ExecutionStateRef*, Args...>(nullptr, args...);

        return executeFunction(p, [](ExecutionStateRef* state, void* tuplePtr, void* fnPtr) -> ValueRef* {
            std::tuple<ExecutionStateRef*, Args...>* tuple = (std::tuple<ExecutionStateRef*, Args...>*)tuplePtr;
            Closure fn = (Closure)fnPtr;

            std::get<0>(*tuple) = state;

            return EvaluatorUtil::applyTupleIntoArgumentsOfVariadicTemplateFunction(fn, *tuple);
        },
                               &tuple, (void*)fn);
    }
};

// temporally disable StackOverflow check
// StackOverflowDisabler only unlocks the predefined stack limit (STACK_USAGE_LIMIT: 4MB)
// should be carefully used because it cannot prevent the system-stackoverflow exception
class ESCARGOT_EXPORT StackOverflowDisabler {
public:
    StackOverflowDisabler(ExecutionStateRef*);
    ~StackOverflowDisabler();

private:
    ExecutionStateRef* m_executionState;
    size_t m_originStackLimit;
};

// Don't save pointer of ExecutionStateRef anywhere yourself
// If you want to acquire ExecutionStateRef, you can use Evaluator::execute
class ESCARGOT_EXPORT ExecutionStateRef {
public:
    OptionalRef<FunctionObjectRef> resolveCallee(); // resolve nearest callee if exists
    GCManagedVector<FunctionObjectRef*> resolveCallstack(); // resolve list of callee
    GlobalObjectRef* resolveCallerLexicalGlobalObject(); // resolve caller's lexical global object

    // only enabled when `ENABLE_EXTENDED_API` macro is set (default: disabled)
    bool onTry();
    bool onCatch();
    bool onFinally();

    void throwException(ValueRef* value);
    void checkStackOverflow();

    GCManagedVector<Evaluator::StackTraceData> computeStackTrace();

    ContextRef* context();

    OptionalRef<ExecutionStateRef> parent();
};

class ESCARGOT_EXPORT VMInstanceRef {
public:
    // you can to provide timezone as TZ database name like "US/Pacific".
    // if you don't provide, we try to detect system timezone.
    static PersistentRefHolder<VMInstanceRef> create(const char* locale = nullptr, const char* timezone = nullptr, const char* baseCacheDir = nullptr);

    typedef void (*OnVMInstanceDelete)(VMInstanceRef* instance);
    void setOnVMInstanceDelete(OnVMInstanceDelete cb);

    // register ErrorCallback which is triggered when each Error constructor (e.g. new TypeError()) invoked or thrown
    // parameter `err` stands for the newly created ErrorObject
    // these functions are used only for third party usage
    // only enabled when `ENABLE_EXTENDED_API` macro is set (default: disabled)
    typedef void (*ErrorCallback)(ExecutionStateRef* state, ErrorObjectRef* err);
    void registerErrorCreationCallback(ErrorCallback cb);
    void registerErrorThrowCallback(ErrorCallback cb);
    void unregisterErrorCreationCallback();
    void unregisterErrorThrowCallback();

    enum PromiseHookType {
        Init,
        Resolve,
        Before,
        After
    };

    enum PromiseRejectEvent {
        PromiseRejectWithNoHandler = 0,
        PromiseHandlerAddedAfterReject = 1,
        PromiseRejectAfterResolved = 2,
        PromiseResolveAfterResolved = 3,
    };

    typedef void (*PromiseHook)(ExecutionStateRef* state, PromiseHookType type, PromiseObjectRef* promise, ValueRef* parent);
    typedef void (*PromiseRejectCallback)(ExecutionStateRef* state, PromiseObjectRef* promise, ValueRef* value, PromiseRejectEvent event);

    // Register PromiseHook (PromiseHook is used by third party app)
    void registerPromiseHook(PromiseHook promiseHook);
    void unregisterPromiseHook();
    // Register a callback to call if this promise is rejected, but it does not have a reject handler
    void registerPromiseRejectCallback(PromiseRejectCallback);
    void unregisterPromiseRejectCallback();

    // this function enforce do gc,
    // remove every compiled bytecodes,
    // remove regexp cache,
    // and compress every comressible strings if we can
    void enterIdleMode();
    // force clear every caches related with context
    // you can call this function if you don't want to use every alive contexts
    void clearCachesRelatedWithContext();

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

    bool hasPendingJob();
    Evaluator::EvaluatorResult executePendingJob();

    bool hasPendingJobFromAnotherThread();
    bool waitEventFromAnotherThread(unsigned timeoutInMillisecond = 0); // zero means infinity
    void executePendingJobFromAnotherThread();

    size_t maxCompiledByteCodeSize();
    void setMaxCompiledByteCodeSize(size_t s);

    bool isCodeCacheEnabled();
    size_t codeCacheMinSourceLength();
    void setCodeCacheMinSourceLength(size_t s);
    size_t codeCacheMaxCacheCount();
    void setCodeCacheMaxCacheCount(size_t s);
    bool codeCacheShouldLoadFunctionOnScriptLoading();
    void setCodeCacheShouldLoadFunctionOnScriptLoading(bool s);
};

class ESCARGOT_EXPORT DebuggerOperationsRef {
public:
    class WeakCodeRef;

    struct BreakpointLocation {
        BreakpointLocation(uint32_t line, uint32_t offset)
            : line(line)
            , offset(offset)
        {
        }

        uint32_t line;
        uint32_t offset;
    };

    typedef std::vector<BreakpointLocation> BreakpointLocationVector;

    struct BreakpointLocationsInfo {
        BreakpointLocationsInfo(WeakCodeRef* weakCodeRef)
            : weakCodeRef(weakCodeRef)
        {
        }

        // The codeRef is a unique id which is not garbage collected
        // to avoid keeping script / function code in the memory.
        WeakCodeRef* weakCodeRef;
        BreakpointLocationVector breakpointLocations;
    };

    struct DebuggerStackTraceData {
        DebuggerStackTraceData(WeakCodeRef* weakCodeRef, size_t line, size_t column, size_t depth)
            : weakCodeRef(weakCodeRef)
            , line(line)
            , column(column)
            , depth(depth)
        {
        }

        WeakCodeRef* weakCodeRef;
        size_t line;
        size_t column;
        size_t depth;
    };

    typedef std::vector<DebuggerStackTraceData> DebuggerStackTraceDataVector;

    enum ScopeType {
        UNKNOWN_ENVIRONMENT,
        GLOBAL_ENVIRONMENT,
        FUNCTION_ENVIRONMENT,
        DECLARATIVE_ENVIRONMENT,
        OBJECT_ENVIRONMENT,
        MODULE_ENVIRONMENT,
    };

    typedef std::vector<ScopeType> LexicalScopeChainVector;

    struct PropertyKeyValue {
        PropertyKeyValue()
            : key(nullptr)
            , value()
        {
        }

        PropertyKeyValue(StringRef* key, OptionalRef<ValueRef> value)
            : key(key)
            , value(value)
        {
        }

        StringRef* key;
        OptionalRef<ValueRef> value;
    };

    typedef GCManagedVector<PropertyKeyValue> PropertyKeyValueVector;

    class ESCARGOT_EXPORT BreakpointOperations {
        friend class DebuggerC;

    public:
        WeakCodeRef* weakCodeRef()
        {
            return m_weakCodeRef;
        }

        ExecutionStateRef* executionState()
        {
            return m_executionState;
        }

        uint32_t offset()
        {
            return m_offset;
        }

        // eval notes:
        //   - error: the result is the string of the error message, isError is set to true
        //   - object: the returned string is nullptr, objectRef contains the index of the object
        //   - other values: the returned string contains the string representation of the value
        StringRef* eval(StringRef* sourceCode, bool& isError, size_t& objectIndex);
        void getStackTrace(DebuggerStackTraceDataVector& outStackTrace);
        void getLexicalScopeChain(uint32_t stateIndex, LexicalScopeChainVector& outLexicalScopeChain);
        PropertyKeyValueVector getLexicalScopeChainProperties(uint32_t stateIndex, uint32_t scopeIndex);
        // Temporary object store for storing inspected objects. All object has an index.
        size_t putObject(ObjectRef* object);
        ObjectRef* getObject(size_t index);

    private:
        class ObjectStore;

        BreakpointOperations(WeakCodeRef* weakCodeRef, ExecutionStateRef* executionState, uint32_t offset)
            : m_weakCodeRef(weakCodeRef)
            , m_executionState(executionState)
            , m_objectStore(nullptr)
            , m_offset(offset)
        {
        }

        WeakCodeRef* m_weakCodeRef;
        ExecutionStateRef* m_executionState;
        ObjectStore* m_objectStore;
        uint32_t m_offset;
    };

    enum ResumeBreakpointOperation {
        Continue,
        Step,
        Next,
        Finish,
    };

    // Base class for debugger callbacks
    class DebuggerClient {
    public:
        virtual ~DebuggerClient(){};
        virtual void parseCompleted(StringRef* source, StringRef* srcName, std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>& breakpointLocationsVector) = 0;
        virtual void parseError(StringRef* source, StringRef* srcName, StringRef* error) = 0;
        virtual void codeReleased(WeakCodeRef* weakCodeRef) = 0;

        virtual ResumeBreakpointOperation stopAtBreakpoint(BreakpointOperations& operations) = 0;
    };

    static StringRef* getFunctionName(WeakCodeRef* weakCodeRef);
    // Returns true, if the breakpoint status is changed from enabled to disabled or vica versa
    static bool updateBreakpoint(WeakCodeRef* weakCodeRef, uint32_t offset, bool enable);
};

class ESCARGOT_EXPORT ContextRef {
public:
    static PersistentRefHolder<ContextRef> create(VMInstanceRef* vmInstance);

    void clearRelatedQueuedJobs();

    VMInstanceRef* vmInstance();
    ScriptParserRef* scriptParser();

    GlobalObjectRef* globalObject();
    ObjectRef* globalObjectProxy();
    // this setter try to update `globalThis` value on GlobalObject
    void setGlobalObjectProxy(ObjectRef* newGlobalObjectProxy);

    bool canThrowException();
    void throwException(ValueRef* exceptionValue);

    bool initDebugger(DebuggerOperationsRef::DebuggerClient* debuggerClient);
    bool disableDebugger();
    // available options(separator is ';')
    // "--port=6501", default for TCP debugger
    bool initDebuggerRemote(const char* options);
    bool isDebuggerRunning();
    bool isWaitBeforeExit();
    void printDebugger(StringRef* output);
    void pumpDebuggerEvents();
    void setAsAlwaysStopState();
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

    bool isStoredInHeap();
    bool isBoolean();
    bool isNumber();
    bool isNull();
    bool isUndefined();
    bool isInt32();
    bool isUInt32();
    bool isDouble();
    bool isTrue();
    bool isFalse();
    bool isCallable(); // can ValueRef::call
    bool isConstructible(); // can ValueRef::construct
    bool isUndefinedOrNull()
    {
        return isUndefined() || isNull();
    }
    bool isPointerValue();

    bool isArgumentsObject();
    bool isArrayPrototypeObject();
    bool isGeneratorFunctionObject();
    bool isGeneratorObject();
    bool isMapIteratorObject();
    bool isSetIteratorObject();
    bool isTypedArrayObject();
    bool isTypedArrayPrototypeObject();
    bool isModuleNamespaceObject();

    bool asBoolean();
    double asNumber();
    int32_t asInt32();
    uint32_t asUInt32();
    PointerValueRef* asPointerValue();

#define DEFINE_VALUEREF_IS_AS(Name) \
    bool is##Name();                \
    Name##Ref* as##Name();

    ESCARGOT_POINTERVALUE_CHILD_REF_LIST(DEFINE_VALUEREF_IS_AS);
    ESCARGOT_TYPEDARRAY_REF_LIST(DEFINE_VALUEREF_IS_AS);
#undef DEFINE_VALUEREF_IS_AS

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
    ValueIndex tryToUseAsIndex(ExecutionStateRef* state);

    enum : uint32_t {
        InvalidIndex32Value = std::numeric_limits<uint32_t>::max(),
        InvalidIndexPropertyValue = InvalidIndex32Value
    };
    uint32_t toIndex32(ExecutionStateRef* state);
    uint32_t tryToUseAsIndex32(ExecutionStateRef* state);
    uint32_t tryToUseAsIndexProperty(ExecutionStateRef* state);

    bool abstractEqualsTo(ExecutionStateRef* state, const ValueRef* other) const; // ==
    bool equalsTo(ExecutionStateRef* state, const ValueRef* other) const; // ===
    bool instanceOf(ExecutionStateRef* state, const ValueRef* other) const;
    ValueRef* call(ExecutionStateRef* state, ValueRef* receiver, const size_t argc, ValueRef** argv);
    ValueRef* construct(ExecutionStateRef* state, const size_t argc, ValueRef** argv); // same with new expression in js
    // call constrictor with already created object
    void callConstructor(ExecutionStateRef* state, ObjectRef* receiver, const size_t argc, ValueRef** argv);
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
    static StringRef* createFromASCII(const char* s, size_t stringLength);
    template <size_t N>
    static StringRef* createFromUTF8(const char (&str)[N])
    {
        return createFromUTF8(str, N - 1);
    }
    static StringRef* createFromUTF8(const char* s, size_t byteLength, bool maybeASCII = true);
    static StringRef* createFromUTF16(const char16_t* s, size_t stringLength);
    static StringRef* createFromLatin1(const unsigned char* s, size_t stringLength);

    static StringRef* createExternalFromASCII(const char* s, size_t stringLength);
    static StringRef* createExternalFromLatin1(const unsigned char* s, size_t stringLength);
    static StringRef* createExternalFromUTF16(const char16_t* s, size_t stringLength);

    // you can use these functions only if you enabled string compression
    static bool isCompressibleStringEnabled();
    static StringRef* createFromUTF8ToCompressibleString(VMInstanceRef* instance, const char* s, size_t byteLength, bool maybeASCII = true);
    static StringRef* createFromUTF16ToCompressibleString(VMInstanceRef* instance, const char16_t* s, size_t stringLength);
    static StringRef* createFromASCIIToCompressibleString(VMInstanceRef* instance, const char* s, size_t stringLength);
    static StringRef* createFromLatin1ToCompressibleString(VMInstanceRef* instance, const unsigned char* s, size_t stringLength);
    static void* allocateStringDataBufferForCompressibleString(size_t byteLength);
    static void deallocateStringDataBufferForCompressibleString(void* ptr, size_t byteLength);
    static StringRef* createFromAlreadyAllocatedBufferToCompressibleString(VMInstanceRef* instance, void* buffer, size_t stringLen, bool is8Bit /* is ASCII or Latin1 */);

    // you can use these functions only if you enabled reloadable string
    static bool isReloadableStringEnabled();
    static StringRef* createReloadableString(VMInstanceRef* instance,
                                             bool is8BitString, size_t stringLength, void* callbackData,
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
    bool equalsWithASCIIString(const char* buf, size_t stringLength);

    StringRef* substring(size_t from, size_t to);

    enum WriteOptions {
        NoOptions = 0,
        ReplaceInvalidUtf8 = 1 << 3
    };

    std::string toStdUTF8String(int options = NoOptions);

    // don't store this sturct or string buffer
    // this is only for temporary access
    struct ESCARGOT_EXPORT StringBufferAccessDataRef {
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
    static SymbolRef* create(OptionalRef<StringRef> desc);
    static SymbolRef* fromGlobalSymbolRegistry(VMInstanceRef* context, StringRef* desc); // this is same with Symbol.for
    StringRef* descriptionString();
    ValueRef* descriptionValue();
    StringRef* symbolDescriptiveString();
};

class ESCARGOT_EXPORT BigIntRef : public PointerValueRef {
public:
    static BigIntRef* create(StringRef* desc, int radix = 10);
    static BigIntRef* create(int64_t num);
    static BigIntRef* create(uint64_t num);

    StringRef* toString(int radix = 10);
    double toNumber();
    int64_t toInt64();
    uint64_t toUint64();

    bool equals(BigIntRef* b);
    bool equals(StringRef* s);
    bool equals(double b);

    bool lessThan(BigIntRef* b);
    bool lessThanEqual(BigIntRef* b);
    bool greaterThan(BigIntRef* b);
    bool greaterThanEqual(BigIntRef* b);

    BigIntRef* addition(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* subtraction(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* multiply(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* division(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* remainder(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* pow(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* bitwiseAnd(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* bitwiseOr(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* bitwiseXor(ExecutionStateRef* state, BigIntRef* b);
    BigIntRef* leftShift(ExecutionStateRef* state, BigIntRef* c);
    BigIntRef* rightShift(ExecutionStateRef* state, BigIntRef* c);

    BigIntRef* increment(ExecutionStateRef* state);
    BigIntRef* decrement(ExecutionStateRef* state);
    BigIntRef* bitwiseNot(ExecutionStateRef* state);
    BigIntRef* negativeValue(ExecutionStateRef* state);

    bool isZero();
    bool isNaN();
    bool isInfinity();
    bool isNegative();
};

class ESCARGOT_EXPORT ObjectPropertyDescriptorRef {
public:
    ObjectPropertyDescriptorRef(ValueRef* value);
    ObjectPropertyDescriptorRef(ValueRef* value, bool writable);
    ObjectPropertyDescriptorRef(ValueRef* getter, ValueRef* setter);

    ObjectPropertyDescriptorRef(const ObjectPropertyDescriptorRef& src);
    const ObjectPropertyDescriptorRef& operator=(const ObjectPropertyDescriptorRef& src);
    ~ObjectPropertyDescriptorRef();

    ValueRef* value() const;
    bool hasValue() const;

    ValueRef* getter() const;
    bool hasGetter() const;
    ValueRef* setter() const;
    bool hasSetter() const;

    bool isEnumerable() const;
    void setEnumerable(bool enumerable);
    bool hasEnumerable() const;

    bool isConfigurable() const;
    void setConfigurable(bool configurable);
    bool hasConfigurable() const;

    bool isWritable() const;
    bool hasWritable() const;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
    void operator delete(void* ptr);
    void operator delete[](void* obj) = delete;

private:
    friend class ObjectWithPropertyHandler;
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
    static ObjectRef* create(ExecutionStateRef* state, ObjectRef* proto);
    // can not redefine or delete virtual property
    // virtual property does not follow every rule of ECMAScript
    static ObjectRef* createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback);

    ValueRef* get(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    ValueRef* getOwnPropertyDescriptor(ExecutionStateRef* state, ValueRef* propertyName);

    bool set(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value);

    ValueRef* getIndexedProperty(ExecutionStateRef* state, ValueRef* property);
    bool setIndexedProperty(ExecutionStateRef* state, ValueRef* property, ValueRef* value);
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
    typedef ValueRef* (*NativeDataAccessorPropertyGetter)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, NativeDataAccessorPropertyData* data);
    typedef bool (*NativeDataAccessorPropertySetter)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, NativeDataAccessorPropertyData* data, ValueRef* setterInputData);
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
    bool defineNativeDataAccessorProperty(ExecutionStateRef* state, ValueRef* P, NativeDataAccessorPropertyData* data, bool actsLikeJSGetterSetter = false);

    bool deleteOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);
    bool hasOwnProperty(ExecutionStateRef* state, ValueRef* propertyName);

    bool deleteProperty(ExecutionStateRef* state, ValueRef* propertyName);
    bool hasProperty(ExecutionStateRef* state, ValueRef* propertyName);

    ValueRef* getPrototype(ExecutionStateRef* state);
    OptionalRef<ObjectRef> getPrototypeObject(ExecutionStateRef* state); // if __proto__ is not object(undefined or null), this function returns nullptr instead of orginal value.
    bool setPrototype(ExecutionStateRef* state, ValueRef* value);
    bool setObjectPrototype(ExecutionStateRef* state, ValueRef* value); // explicitly call Object::setPrototype

    StringRef* constructorName(ExecutionStateRef* state);

    OptionalRef<ContextRef> creationContext();

    ValueVectorRef* ownPropertyKeys(ExecutionStateRef* state);

    void enumerateObjectOwnProperties(ExecutionStateRef* state, const std::function<bool(ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>& cb, bool shouldSkipSymbolKey = true);

    // get `length` property like ToLength(Get(obj, "length"))
    // it returns 0 for exceptional cases (e.g. undefined, nan)
    uint64_t length(ExecutionStateRef* state);
    void setLength(ExecutionStateRef* state, uint64_t newLength);

    IteratorObjectRef* values(ExecutionStateRef* state);
    IteratorObjectRef* keys(ExecutionStateRef* state);
    IteratorObjectRef* entries(ExecutionStateRef* state);

    bool isExtensible(ExecutionStateRef* state);
    bool preventExtensions(ExecutionStateRef* state);
    bool setIntegrityLevel(ExecutionStateRef* state, bool isSealed);

    void* extraData();
    void setExtraData(void* e);
    // this function is used only for test purpose
    void setIsHTMLDDA();

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
    FunctionObjectRef* aggregateError();
    ObjectRef* aggregateErrorPrototype();
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
    FunctionObjectRef* promiseAll();
    FunctionObjectRef* promiseAllSettled();
    FunctionObjectRef* promiseAny();
    FunctionObjectRef* promiseRace();
    FunctionObjectRef* promiseReject();
    FunctionObjectRef* promiseResolve();
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
    ObjectRef* bigInt64Array();
    ObjectRef* bigInt64ArrayPrototype();
    ObjectRef* bigUint64Array();
    ObjectRef* bigUint64ArrayPrototype();
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
    typedef ValueRef* (*NativeFunctionPointer)(ExecutionStateRef* state, ValueRef* thisValue,
                                               size_t argc, ValueRef** argv, bool isConstructorCall);
    typedef ValueRef* (*NativeFunctionWithNewTargetPointer)(ExecutionStateRef* state, ValueRef* thisValue,
                                                            size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget);

    enum BuiltinFunctionSlot : size_t {
        PublicFunctionIndex = 0,
    };

    struct ESCARGOT_EXPORT NativeFunctionInfo {
        bool m_isStrict;
        bool m_isConstructor;
        bool m_hasWithNewTargetCallback;
        AtomicStringRef* m_name;
        NativeFunctionPointer m_nativeFunction;
        NativeFunctionWithNewTargetPointer m_nativeFunctionWithNewTarget;
        size_t m_argumentCount;

        NativeFunctionInfo(AtomicStringRef* name, NativeFunctionPointer fn, size_t argc, bool isStrict = true, bool isConstructor = true)
            : m_isStrict(isStrict)
            , m_isConstructor(isConstructor)
            , m_hasWithNewTargetCallback(false)
            , m_name(name)
            , m_nativeFunction(fn)
            , m_nativeFunctionWithNewTarget(nullptr)
            , m_argumentCount(argc)
        {
        }

        NativeFunctionInfo(AtomicStringRef* name, NativeFunctionWithNewTargetPointer fn, size_t argc, bool isStrict = true, bool isConstructor = true)
            : m_isStrict(isStrict)
            , m_isConstructor(isConstructor)
            , m_hasWithNewTargetCallback(true)
            , m_name(name)
            , m_nativeFunction(nullptr)
            , m_nativeFunctionWithNewTarget(fn)
            , m_argumentCount(argc)
        {
        }
    };

    static FunctionObjectRef* create(ExecutionStateRef* state, NativeFunctionInfo info);
    static FunctionObjectRef* createBuiltinFunction(ExecutionStateRef* state, NativeFunctionInfo info); // protoype of builtin function is non-writable

    // dynamically create a function from source string
    static FunctionObjectRef* create(ExecutionStateRef* state, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* body);
    static FunctionObjectRef* create(ExecutionStateRef* state, StringRef* sourceName, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* body);

    // returns associate context
    ContextRef* context();

    // get prototype property of constructible function(not [[prototype]])
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    ValueRef* getFunctionPrototype(ExecutionStateRef* state);

    // set prototype property constructible function(not [[prototype]])
    // this property is used for new object construction. see https://www.ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
    bool setFunctionPrototype(ExecutionStateRef* state, ValueRef* v);

    bool isConstructor();
    void markFunctionNeedsSlowVirtualIdentifierOperation();

    // set function name is allowed only for native function or dynamically created function except class constructor
    bool setName(AtomicStringRef* name);
};

class ESCARGOT_EXPORT IteratorObjectRef : public ObjectRef {
public:
    ValueRef* next(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT GenericIteratorObjectRef : public IteratorObjectRef {
public:
    // returns result and done pair
    typedef std::pair<ValueRef*, bool> (*GenericIteratorObjectRefCallback)(ExecutionStateRef* state, void* data);

    static GenericIteratorObjectRef* create(ExecutionStateRef* state, GenericIteratorObjectRefCallback callback, void* callbackData);
};

class ESCARGOT_EXPORT ArrayObjectRef : public ObjectRef {
public:
    static ArrayObjectRef* create(ExecutionStateRef* state);
    static ArrayObjectRef* create(ExecutionStateRef* state, const uint64_t size);
    static ArrayObjectRef* create(ExecutionStateRef* state, ValueVectorRef* source);
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
        EvalError,
        AggregateError,
    };
    static ErrorObjectRef* create(ExecutionStateRef* state, ErrorObjectRef::Code code, StringRef* errorMessage);
    void updateStackTraceData(ExecutionStateRef* state); // update stacktrace data
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

class ESCARGOT_EXPORT AggregateErrorObjectRef : public ErrorObjectRef {
public:
    static AggregateErrorObjectRef* create(ExecutionStateRef* state, StringRef* errorMessage);
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

class ESCARGOT_EXPORT BigIntObjectRef : public ObjectRef {
public:
    static BigIntObjectRef* create(ExecutionStateRef* state);

    void setPrimitiveValue(ExecutionStateRef* state, BigIntRef* value);
    BigIntRef* primitiveValue();
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
        HasIndices = 1 << 0,
        Global = 1 << 1,
        IgnoreCase = 1 << 2,
        MultiLine = 1 << 3,
        DotAll = 1 << 4,
        Unicode = 1 << 5,
        UnicodeSets = 1 << 6,
        Sticky = 1 << 7,
    };

    struct ESCARGOT_EXPORT RegexMatchResult {
        struct RegexMatchResultPiece {
            unsigned m_start, m_end;
        };
        int m_subPatternNum;
        std::vector<std::vector<RegexMatchResultPiece>> m_matchResults;
    };

    static RegExpObjectRef* create(ExecutionStateRef* state, ValueRef* source, ValueRef* option);
    static RegExpObjectRef* create(ExecutionStateRef* state, ValueRef* source, RegExpObjectOption option = None);

    bool match(ExecutionStateRef* state, ValueRef* str, RegexMatchResult& result, bool testOnly = false, size_t startIndex = 0);

    StringRef* source();
    RegExpObjectOption option();
};

class ESCARGOT_EXPORT BackingStoreRef {
    friend class ArrayBufferObject;

public:
    typedef void (*BackingStoreRefDeleterCallback)(void* data, size_t length,
                                                   void* deleterData);

    // create default NonSharedBackingStore allocated by platform allocator
    static BackingStoreRef* createDefaultNonSharedBackingStore(size_t byteLength);
    // create customized NonSharedBackingStore allocated by other allocator
    static BackingStoreRef* createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreRefDeleterCallback callback, void* callbackData);

    // Note) SharedBackingStore is allocated for each worker(thread) and its internal data block is actually shared among workers.
    // Also, SharedBackingStore's internal data block is allocated only by platform allocator now.
    // create default SharedBackingStore allocated by platform allocator
    static BackingStoreRef* createDefaultSharedBackingStore(size_t byteLength);
    // create SharedBackingStore by sharing with already created one
    static BackingStoreRef* createSharedBackingStore(BackingStoreRef* backingStore);

    void* data();
    size_t byteLength();
    // Indicates whether the backing store is SharedDataBlock(for SharedArrayBuffer)
    bool isShared();
    void reallocate(size_t newByteLength);
};

class ESCARGOT_EXPORT ArrayBufferRef : public ObjectRef {
public:
    OptionalRef<BackingStoreRef> backingStore();
    uint8_t* rawBuffer();
    size_t byteLength();
};

class ESCARGOT_EXPORT ArrayBufferObjectRef : public ArrayBufferRef {
public:
    static ArrayBufferObjectRef* create(ExecutionStateRef* state);
    void allocateBuffer(ExecutionStateRef* state, size_t bytelength);
    void attachBuffer(BackingStoreRef* backingStore);
    void detachArrayBuffer();
    bool isDetachedBuffer();
};

class ESCARGOT_EXPORT SharedArrayBufferObjectRef : public ArrayBufferRef {
public:
    static SharedArrayBufferObjectRef* create(ExecutionStateRef* state, size_t bytelength);
    static SharedArrayBufferObjectRef* create(ExecutionStateRef* state, BackingStoreRef* backingStore);
};

class ESCARGOT_EXPORT ArrayBufferViewRef : public ObjectRef {
public:
    ArrayBufferRef* buffer();
    void setBuffer(ArrayBufferRef* bo, size_t byteOffset, size_t byteLength, size_t arrayLength);
    void setBuffer(ArrayBufferRef* bo, size_t byteOffset, size_t byteLength);
    uint8_t* rawBuffer();
    size_t byteLength();
    size_t byteOffset();
    size_t arrayLength();
};

class ESCARGOT_EXPORT DataViewObjectRef : public ArrayBufferViewRef {
public:
    static DataViewObjectRef* create(ExecutionStateRef* state);
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

class ESCARGOT_EXPORT BigInt64ArrayObjectRef : public ArrayBufferViewRef {
public:
    static BigInt64ArrayObjectRef* create(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT BigUint64ArrayObjectRef : public ArrayBufferViewRef {
public:
    static BigUint64ArrayObjectRef* create(ExecutionStateRef* state);
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
    ObjectRef* then(ExecutionStateRef* state, ValueRef* onFulfilled, ValueRef* onRejected);
    ObjectRef* catchOperation(ExecutionStateRef* state, ValueRef* handler);

    void fulfill(ExecutionStateRef* state, ValueRef* value);
    void reject(ExecutionStateRef* state, ValueRef* reason);

    bool hasResolveHandlers();
    bool hasRejectHandlers();
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
};

class ESCARGOT_EXPORT WeakSetObjectRef : public ObjectRef {
public:
    static WeakSetObjectRef* create(ExecutionStateRef* state);
    bool add(ExecutionStateRef* state, ValueRef* key);
    bool deleteOperation(ExecutionStateRef* state, ValueRef* key);
    bool has(ExecutionStateRef* state, ValueRef* key);
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
};

class ESCARGOT_EXPORT WeakMapObjectRef : public ObjectRef {
public:
    static WeakMapObjectRef* create(ExecutionStateRef* state);
    bool deleteOperation(ExecutionStateRef* state, ValueRef* key);
    ValueRef* get(ExecutionStateRef* state, ValueRef* key);
    bool has(ExecutionStateRef* state, ValueRef* key);
    void set(ExecutionStateRef* state, ValueRef* key, ValueRef* value);
};

class ESCARGOT_EXPORT WeakRefObjectRef : public ObjectRef {
public:
    static OptionalRef<WeakRefObjectRef> create(ExecutionStateRef* state, ValueRef* target);
    // returns true if target is alive
    bool deleteOperation(ExecutionStateRef* state);
    OptionalRef<PointerValueRef> deref();
};

class ESCARGOT_EXPORT FinalizationRegistryObjectRef : public ObjectRef {
public:
    static FinalizationRegistryObjectRef* create(ExecutionStateRef* state, ObjectRef* cleanupCallback, ContextRef* realm);
};

// don't modify template after instantiate object
// it is not intented operation
// Note) only String or Symbol type is allowed for `propertyName`
// because TemplateRef is set without ExecutionStateRef, so property name conversion is impossible.
// only enabled when `ENABLE_EXTENDED_API` macro is set (default: disabled)
class ESCARGOT_EXPORT TemplateRef {
public:
    void set(ValueRef* propertyName, ValueRef* data, bool isWritable, bool isEnumerable, bool isConfigurable);
    void set(ValueRef* propertyName, TemplateRef* data, bool isWritable, bool isEnumerable, bool isConfigurable);
    void setAccessorProperty(ValueRef* propertyName, OptionalRef<FunctionTemplateRef> getter, OptionalRef<FunctionTemplateRef> setter, bool isEnumerable, bool isConfigurable);
    void setNativeDataAccessorProperty(ValueRef* propertyName, ObjectRef::NativeDataAccessorPropertyGetter getter, ObjectRef::NativeDataAccessorPropertySetter setter,
                                       bool isWritable, bool isEnumerable, bool isConfigurable, bool actsLikeJSGetterSetter = false);
    void setNativeDataAccessorProperty(ValueRef* propertyName, ObjectRef::NativeDataAccessorPropertyData* data, bool actsLikeJSGetterSetter = false);

    bool has(ValueRef* propertyName);
    // return true if removed
    bool remove(ValueRef* propertyName);

    ObjectRef* instantiate(ContextRef* ctx);
    bool didInstantiate();

    bool isObjectTemplate();
    bool isFunctionTemplate();

    void setInstanceExtraData(void* ptr);
    void* instanceExtraData();
};

typedef OptionalRef<ValueRef> (*PropertyHandlerGetterCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*PropertyHandlerSetterCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, ValueRef* value);
enum class ObjectTemplatePropertyAttribute : uint8_t {
    PropertyAttributeNotExist = 1 << 0,
    PropertyAttributeExist = 1 << 1,
    PropertyAttributeWritable = 1 << 2,
    PropertyAttributeEnumerable = 1 << 3,
    PropertyAttributeConfigurable = 1 << 4,
};
inline bool operator&(ObjectTemplatePropertyAttribute a, ObjectTemplatePropertyAttribute b)
{
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}
inline ObjectTemplatePropertyAttribute operator|(ObjectTemplatePropertyAttribute a, ObjectTemplatePropertyAttribute b)
{
    return static_cast<ObjectTemplatePropertyAttribute>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
typedef ObjectTemplatePropertyAttribute (*PropertyHandlerQueryCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*PropertyHandlerDeleteCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName);
typedef ValueVectorRef* (*PropertyHandlerEnumerationCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data);
// if intercepted you may returns non-empty value.
// the returned value will be use futuer operation(you can return true, or false)
typedef OptionalRef<ValueRef> (*PropertyHandlerDefineOwnPropertyCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName, const ObjectPropertyDescriptorRef& desc);
typedef OptionalRef<ValueRef> (*PropertyHandlerGetPropertyDescriptorCallback)(ExecutionStateRef* state, ObjectRef* self, ValueRef* receiver, void* data, ValueRef* propertyName);

struct ESCARGOT_EXPORT ObjectTemplatePropertyHandlerConfiguration {
    PropertyHandlerGetterCallback getter;
    PropertyHandlerSetterCallback setter;
    PropertyHandlerQueryCallback query;
    PropertyHandlerDeleteCallback deleter;
    PropertyHandlerEnumerationCallback enumerator;
    PropertyHandlerDefineOwnPropertyCallback definer;
    PropertyHandlerGetPropertyDescriptorCallback descriptor;
    void* data; // this data member is guaranteed to be kept in GC heap

    ObjectTemplatePropertyHandlerConfiguration(
        PropertyHandlerGetterCallback getter = nullptr,
        PropertyHandlerSetterCallback setter = nullptr,
        PropertyHandlerQueryCallback query = nullptr,
        PropertyHandlerDeleteCallback deleter = nullptr,
        PropertyHandlerEnumerationCallback enumerator = nullptr,
        PropertyHandlerDefineOwnPropertyCallback definer = nullptr,
        PropertyHandlerGetPropertyDescriptorCallback descriptor = nullptr,
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

// only enabled when `ENABLE_EXTENDED_API` macro is set (default: disabled)
class ESCARGOT_EXPORT ObjectTemplateRef : public TemplateRef {
public:
    static ObjectTemplateRef* create();
    void setNamedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data);
    void setIndexedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data);
    void removeNamedPropertyHandler();
    void removeIndexedPropertyHandler();

    // returns function template if object template is instance template of function template
    OptionalRef<FunctionTemplateRef> constructor();

    // returns the installation was successful
    bool installTo(ContextRef* ctx, ObjectRef* target);
};

// FunctionTemplateRef returns the unique function instance in context.
// only enabled when `ENABLE_EXTENDED_API` macro is set (default: disabled)
class ESCARGOT_EXPORT FunctionTemplateRef : public TemplateRef {
public:
    // in constructor call, thisValue is default consturcted object
    typedef ValueRef* (*NativeFunctionPointer)(ExecutionStateRef* state, ValueRef* thisValue, size_t argc, ValueRef** argv, OptionalRef<ObjectRef> newTarget);

    static FunctionTemplateRef* create(AtomicStringRef* name, size_t argumentCount, bool isStrict, bool isConstructor,
                                       FunctionTemplateRef::NativeFunctionPointer fn);

    // setName and setLength should be called before instantiate
    void setName(AtomicStringRef* name);
    void setLength(size_t length);

    void updateCallbackFunction(FunctionTemplateRef::NativeFunctionPointer fn);

    ObjectTemplateRef* prototypeTemplate();
    // ObjectTemplate for new'ed instance of this functionTemplate
    ObjectTemplateRef* instanceTemplate();

    void inherit(OptionalRef<FunctionTemplateRef> parent);
    OptionalRef<FunctionTemplateRef> parent();
};

class ESCARGOT_EXPORT SerializerRef {
public:
    // returns the serialization was successful
    static bool serializeInto(ValueRef* value, std::ostringstream& output);
    static ValueRef* deserializeFrom(ContextRef* context, std::istringstream& input);
};

class ESCARGOT_EXPORT ScriptParserRef {
public:
    struct ESCARGOT_EXPORT InitializeScriptResult {
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

    struct ESCARGOT_EXPORT InitializeFunctionScriptResult {
        bool isSuccessful()
        {
            return script.hasValue();
        }

        OptionalRef<ScriptRef> script;
        OptionalRef<FunctionObjectRef> functionObject;

        StringRef* parseErrorMessage;
        ErrorObjectRef::Code parseErrorCode;

        InitializeFunctionScriptResult();
    };

    // parse the input source code and return the result (Script)
    InitializeScriptResult initializeScript(StringRef* sourceCode, StringRef* srcName, bool isModule = false);
    // convert the input body source into a function and parse it
    // generate Script and FunctionObject
    InitializeFunctionScriptResult initializeFunctionScript(StringRef* sourceName, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* functionBody);
    // parse the input JSON data and return the result (Script)
    InitializeScriptResult initializeJSONModule(StringRef* sourceCode, StringRef* srcName);
};

class ESCARGOT_EXPORT ScriptRef {
public:
    bool isModule();
    bool isExecuted();
    StringRef* src();
    StringRef* sourceCode();
    ContextRef* context();
    ValueRef* execute(ExecutionStateRef* state);

    // only module can use these functions
    size_t moduleRequestsLength();
    StringRef* moduleRequest(size_t i);
    ObjectRef* moduleNamespace(ExecutionStateRef* state);
    bool wasThereErrorOnModuleEvaluation();
    ValueRef* moduleEvaluationError();
    enum ModuleStatus {
        Uninstantiated,
        Instantiating,
        Instantiated,
        Evaluating,
        Evaluated,
        Errored
    };
    ModuleStatus moduleStatus();
    ValueRef* moduleInstantiate(ExecutionStateRef* state);
    ValueRef* moduleEvaluate(ExecutionStateRef* state);
};

class ESCARGOT_EXPORT PlatformRef {
public:
    virtual ~PlatformRef() {}

    // ArrayBuffer
    // client must returns zero-filled memory
    virtual void* onMallocArrayBufferObjectDataBuffer(size_t sizeInByte)
    {
        return calloc(sizeInByte, 1);
    }
    virtual void onFreeArrayBufferObjectDataBuffer(void* buffer, size_t sizeInByte, void* deleterData)
    {
        return free(buffer);
    }

    virtual void* onReallocArrayBufferObjectDataBuffer(void* oldBuffer, size_t oldSizeInByte, size_t newSizeInByte)
    {
        void* ptr = realloc(oldBuffer, newSizeInByte);
        if (oldSizeInByte < newSizeInByte) {
            uint8_t* s = static_cast<uint8_t*>(ptr);
            memset(s + oldSizeInByte, 0, newSizeInByte - oldSizeInByte);
        }
        return ptr;
    }

    // If you want to add a Job event, you should call VMInstanceRef::executePendingJob after event. see Shell.cpp
    virtual void markJSJobEnqueued(ContextRef* relatedContext) = 0;

    // may called from another thread. ex) notify or timed out of Atomics.waitAsync
    virtual void markJSJobFromAnotherThreadExists(ContextRef* relatedContext) = 0;

    // Module
    enum ModuleType {
        ModuleES,
        ModuleJSON,
    };

    // client needs cache module map<absolute_module_path, ScriptRef*>
    struct ESCARGOT_EXPORT LoadModuleResult {
        LoadModuleResult(ScriptRef* result);
        LoadModuleResult(ErrorObjectRef::Code errorCode, StringRef* errorMessage);

        OptionalRef<ScriptRef> script;
        StringRef* errorMessage;
        ErrorObjectRef::Code errorCode;
    };
    virtual LoadModuleResult onLoadModule(ContextRef* relatedContext, ScriptRef* whereRequestFrom, StringRef* moduleSrc, ModuleType type) = 0;
    virtual void didLoadModule(ContextRef* relatedContext, OptionalRef<ScriptRef> whereRequestFrom, ScriptRef* loadedModule) = 0;

    // Dynamic Import
    virtual void hostImportModuleDynamically(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, ModuleType type, PromiseObjectRef* promise)
    {
        notifyHostImportModuleDynamicallyResult(relatedContext, referrer, src, promise, onLoadModule(relatedContext, referrer, src, type));
    }
    void notifyHostImportModuleDynamicallyResult(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, PromiseObjectRef* promise, LoadModuleResult loadModuleResult);

    virtual bool canBlockExecution(ContextRef* relatedContext)
    {
        return true;
    }

    // ThreadLocal custom data
    // PlatformRef should not have any member variables
    // Instead, user could allocate thread-local values through following methods
    virtual void* allocateThreadLocalCustomData()
    {
        // do nothing
        return nullptr;
    }

    virtual void deallocateThreadLocalCustomData()
    {
        // do nothing
    }

    // you can use these functions only if you enabled custom logging
    static bool isCustomLoggingEnabled();
    // default custom logger
    virtual void customInfoLogger(const char* format, va_list arg)
    {
        vfprintf(stdout, format, arg);
    }

    virtual void customErrorLogger(const char* format, va_list arg)
    {
        vfprintf(stderr, format, arg);
    }

    void* threadLocalCustomData();
};

class ESCARGOT_EXPORT WASMOperationsRef {
public:
    // you can use these functions only if you enabled WASM
    static bool isWASMOperationsEnabled();
    static ValueRef* copyStableBufferBytes(ExecutionStateRef* state, ValueRef* source);
    static ObjectRef* asyncCompileModule(ExecutionStateRef* state, ValueRef* source);
    static ObjectRef* instantiatePromiseOfModuleWithImportObject(ExecutionStateRef* state, PromiseObjectRef* promiseOfModule, ValueRef* importObj);
};

} // namespace Escargot

#endif
