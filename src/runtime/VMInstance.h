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

#ifndef __EscargotVMInstance__
#define __EscargotVMInstance__

#include "runtime/SandBox.h"
#include "runtime/AtomicString.h"
#include "runtime/StaticStrings.h"
#include "runtime/ToStringRecursionPreventer.h"

namespace Escargot {

class Context;
class CodeBlock;
class JobQueue;
class Job;
class Symbol;
class String;
#if defined(ENABLE_COMPRESSIBLE_STRING)
class CompressibleString;
#endif
#if defined(ENABLE_RELOADABLE_STRING)
class ReloadableString;
#endif
#if defined(ENABLE_CODE_CACHE)
class CodeCache;
#endif

#define DEFINE_GLOBAL_SYMBOLS(F) \
    F(hasInstance)               \
    F(isConcatSpreadable)        \
    F(iterator)                  \
    F(species)                   \
    F(split)                     \
    F(toPrimitive)               \
    F(toStringTag)               \
    F(unscopables)               \
    F(search)                    \
    F(match)                     \
    F(matchAll)                  \
    F(replace)                   \
    F(asyncIterator)

struct GlobalSymbols {
#define DECLARE_GLOBAL_SYMBOLS(name) Symbol* name;
    DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS
};

struct GlobalSymbolRegistryItem {
    String* key;
    Symbol* symbol;
};

typedef Vector<GlobalSymbolRegistryItem, GCUtil::gc_malloc_allocator<GlobalSymbolRegistryItem>> GlobalSymbolRegistryVector;

class VMInstance : public gc {
    friend class Context;
    friend class VMInstanceRef;
    friend class ScriptParser;
    friend class SandBox;
    friend void vmMarkStartCallback(void* data);
    friend void vmReclaimEndCallback(void* data);

public:
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

    typedef void (*PromiseHook)(ExecutionState& state, PromiseHookType type, PromiseObject* promise, const Value& parent, void* hook);
    typedef void (*PromiseRejectCallback)(ExecutionState& state, PromiseObject* promise, const Value& value, PromiseRejectEvent event, void* callback);

    VMInstance(const char* locale = nullptr, const char* timezone = nullptr, const char* baseCacheDir = nullptr);
    ~VMInstance();

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    void enterIdleMode();
    void clearCachesRelatedWithContext();

    const GlobalSymbols& globalSymbols()
    {
        return m_globalSymbols;
    }

    GlobalSymbolRegistryVector& globalSymbolRegistry()
    {
        return m_globalSymbolRegistry;
    }

#if defined(ENABLE_ICU)
    const std::string& locale()
    {
        return m_locale;
    }

    UCalendar* calendar()
    {
        if (m_calendar == nullptr) {
            ensureCalendar();
        }
        return m_calendar;
    }

    const std::string& timezoneID()
    {
        ensureTimezoneID();
        return m_timezoneID;
    }
#endif

    const std::string& tzname(size_t i)
    {
        ensureTzname();
        return m_tzname[i];
    }

    DateObject* cachedUTC(ExecutionState& state);

    // object
    // []

    // function
    // [name, length] or [prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // string
    // [length]
    static Value stringLengthNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea);
    static bool stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // regexp
    // [lastIndex]
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea);
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    bool inIdleMode() const
    {
        return m_inIdleMode;
    }

    bool didSomePrototypeObjectDefineIndexedProperty()
    {
        return m_didSomePrototypeObjectDefineIndexedProperty;
    }

    void addObjectStructureToRootSet(ObjectStructure* structure);
    Optional<ObjectStructure*> findRootedObjectStructure(ObjectStructureItem* properties, size_t propertyCount);

    void somePrototypeObjectDefineIndexedProperty(ExecutionState& state);

    ToStringRecursionPreventer& toStringRecursionPreventer()
    {
        return m_toStringRecursionPreventer;
    }

    JobQueue* jobQueue()
    {
        return m_jobQueue;
    }

    void enqueueJob(Job* job);
    bool hasPendingJob();
    SandBox::SandBoxResult executePendingJob();

    bool hasPendingJobFromAnotherThread();
    bool waitEventFromAnotherThread(unsigned timeoutInMillisecond = 0); // zero means infinity
    void executePendingJobFromAnotherThread();

    std::vector<ByteCodeBlock*>& compiledByteCodeBlocks()
    {
        return m_compiledByteCodeBlocks;
    }

    size_t& compiledByteCodeSize()
    {
        return m_compiledByteCodeSize;
    }

    size_t maxCompiledByteCodeSize()
    {
        return m_maxCompiledByteCodeSize;
    }

    void setMaxCompiledByteCodeSize(size_t s)
    {
        m_maxCompiledByteCodeSize = s;
    }

#if defined(ENABLE_COMPRESSIBLE_STRING)
    std::vector<CompressibleString*>& compressibleStrings()
    {
        return m_compressibleStrings;
    }

    size_t& compressibleStringsUncomressedBufferSize()
    {
        return m_compressibleStringsUncomressedBufferSize;
    }
#endif

#if defined(ENABLE_RELOADABLE_STRING)
    std::vector<ReloadableString*>& reloadableStrings()
    {
        return m_reloadableStrings;
    }
#endif

    SandBox* currentSandBox()
    {
        return m_currentSandBox;
    }

    ASCIIString** regexpOptionStringCache()
    {
        return m_regexpOptionStringCache;
    }

    void setOnDestroyCallback(void (*onVMInstanceDestroy)(VMInstance* instance, void* data), void* data)
    {
        m_onVMInstanceDestroy = onVMInstanceDestroy;
        m_onVMInstanceDestroyData = data;
    }

    bool isErrorCreationCallbackRegistered()
    {
        return !!m_errorCreationCallback;
    }

    bool isErrorThrowCallbackRegistered()
    {
        return !!m_errorThrowCallback;
    }

    void registerErrorCreationCallback(void (*ErrorCallback)(ExecutionState& state, ErrorObject* err, void* cb), void* callbackPublic)
    {
        m_errorCreationCallback = ErrorCallback;
        m_errorCreationCallbackPublic = callbackPublic;
    }

    void registerErrorThrowCallback(void (*ErrorCallback)(ExecutionState& state, ErrorObject* err, void* cb), void* callbackPublic)
    {
        m_errorThrowCallback = ErrorCallback;
        m_errorThrowCallbackPublic = callbackPublic;
    }

    void unregisterErrorCreationCallback()
    {
        m_errorCreationCallback = nullptr;
        m_errorCreationCallbackPublic = nullptr;
    }

    void unregisterErrorThrowCallback()
    {
        m_errorThrowCallback = nullptr;
        m_errorThrowCallbackPublic = nullptr;
    }

    void triggerErrorCreationCallback(ExecutionState& state, ErrorObject* error)
    {
        ASSERT(!!m_errorCreationCallback);
        if (m_errorCreationCallbackPublic) {
            m_errorCreationCallback(state, error, m_errorCreationCallbackPublic);
        }
    }

    void triggerErrorThrowCallback(ExecutionState& state, ErrorObject* error)
    {
        ASSERT(!!m_errorThrowCallback);
        if (m_errorThrowCallbackPublic) {
            m_errorThrowCallback(state, error, m_errorThrowCallbackPublic);
        }
    }

    // PromiseHook is triggered for each Promise event
    // Third party app registers PromiseHook when it is necessary
    bool isPromiseHookRegistered()
    {
        return !!m_promiseHook;
    }

    void registerPromiseHook(PromiseHook promiseHook, void* promiseHookPublic)
    {
        m_promiseHook = promiseHook;
        m_promiseHookPublic = promiseHookPublic;
    }

    void unregisterPromiseHook()
    {
        m_promiseHook = nullptr;
        m_promiseHookPublic = nullptr;
    }

    void triggerPromiseHook(ExecutionState& state, PromiseHookType type, PromiseObject* promise, const Value& parent = Value())
    {
        ASSERT(!!m_promiseHook);
        if (m_promiseHookPublic) {
            m_promiseHook(state, type, promise, parent, m_promiseHookPublic);
        }
    }

    // trigger PromiseRejectCallback
    // Third party app registers PromiseRejectCallback when it is necessary
    bool isPromiseRejectCallbackRegistered()
    {
        return !!m_promiseRejectCallback;
    }

    void registerPromiseRejectCallback(PromiseRejectCallback promiseRejectCallback, void* promiseRejectCallbackPublic)
    {
        m_promiseRejectCallback = promiseRejectCallback;
        m_promiseRejectCallbackPublic = promiseRejectCallbackPublic;
    }

    void unregisterPromiseRejectCallback()
    {
        m_promiseRejectCallback = nullptr;
        m_promiseRejectCallbackPublic = nullptr;
    }

    void triggerPromiseRejectCallback(ExecutionState& state, PromiseObject* promise, const Value& value, PromiseRejectEvent event)
    {
        ASSERT(!!m_promiseRejectCallback);
        if (m_promiseRejectCallbackPublic) {
            m_promiseRejectCallback(state, promise, value, event, m_promiseRejectCallbackPublic);
        }
    }

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlCollatorAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlDateTimeFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlNumberFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlRelativeTimeFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlDisplayNamesAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlListFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlPluralRulesAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& caseMappingAvailableLocales();
#endif

#if defined(ENABLE_CODE_CACHE)
    CodeCache* codeCache()
    {
        ASSERT(!!m_codeCache);
        return m_codeCache;
    }
#endif

#if defined(ENABLE_THREADING)
    typedef std::tuple<Context*, Object* /* Promise */, void* /* Global::WaiterListItem */, bool /* notified */, std::shared_ptr<std::thread>> AsyncWaiterDataItem;
    Vector<AsyncWaiterDataItem, GCUtil::gc_malloc_allocator<AsyncWaiterDataItem>>& asyncWaiterData()
    {
        return m_asyncWaiterData;
    }

    std::mutex& asyncWaiterDataMutex()
    {
        return m_asyncWaiterDataMutex;
    }

    std::atomic_size_t& pendingAsyncWaiterCount()
    {
        return m_pendingAsyncWaiterCount;
    }

    std::condition_variable& waitEventFromAnotherThreadConditionVariable()
    {
        return m_waitEventFromAnotherThreadConditionVariable;
    }
#endif

private:
    StaticStrings m_staticStrings;
    AtomicStringMap m_atomicStringMap;
    GlobalSymbols m_globalSymbols;
    GlobalSymbolRegistryVector m_globalSymbolRegistry;
    SandBox* m_currentSandBox;

    bool m_isFinalized;
    bool m_inIdleMode;
    // this flag should affect VM-wide array object
    bool m_didSomePrototypeObjectDefineIndexedProperty;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForBoundFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObjectWithName;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForMappedArgumentsObject;
    ObjectStructure* m_defaultStructureForUnmappedArgumentsObject;

    ObjectPrivateMemberStructure* m_defaultPrivateMemberStructure;

    HashSet<ObjectStructure*, ObjectStructureHash, ObjectStructureEqualTo, GCUtil::gc_malloc_allocator<ObjectStructure*>> m_rootedObjectStructure;

    std::vector<ByteCodeBlock*> m_compiledByteCodeBlocks;
    size_t m_compiledByteCodeSize;
    size_t m_maxCompiledByteCodeSize;

#if defined(ENABLE_COMPRESSIBLE_STRING)
    uint64_t m_lastCompressibleStringsTestTime;
    size_t m_compressibleStringsUncomressedBufferSize;
    std::vector<CompressibleString*> m_compressibleStrings;

    NEVER_INLINE void compressStringsIfNeeds(uint64_t currentTickCount = fastTickCount());
#endif
#if defined(ENABLE_RELOADABLE_STRING)
    std::vector<ReloadableString*> m_reloadableStrings;
#endif

    static void gcEventCallback(GC_EventType t, void* data);
    void (*m_onVMInstanceDestroy)(VMInstance* instance, void* data);
    void* m_onVMInstanceDestroyData;

    void (*m_errorCreationCallback)(ExecutionState& state, ErrorObject* err, void* cb);
    void* m_errorCreationCallbackPublic;
    void (*m_errorThrowCallback)(ExecutionState& state, ErrorObject* err, void* cb);
    void* m_errorThrowCallbackPublic;

    // PromiseHook is triggered for each Promise event
    // Third party app registers PromiseHook when it is necessary
    PromiseHook m_promiseHook;
    void* m_promiseHookPublic;

    // Third party app registers PromiseRejectCallback when it is necessary
    PromiseRejectCallback m_promiseRejectCallback;
    void* m_promiseRejectCallbackPublic;

    ToStringRecursionPreventer m_toStringRecursionPreventer;

    // regexp object data
    RegExpCacheMap* m_regexpCache;
    ASCIIString** m_regexpOptionStringCache;

// date object data
#ifdef ENABLE_ICU
    std::string m_locale;
    UCalendar* m_calendar;
    std::string m_timezoneID;
    void ensureTimezoneID();
    void ensureCalendar();
#endif
    void ensureTzname();
    std::string m_tzname[2];
    DateObject* m_cachedUTC;

    // promise job queue
    JobQueue* m_jobQueue;

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    void ensureIntlSupportedLocales();
    // we expect uloc_, udat_, unum_ returns same supported locales
    Vector<String*, GCUtil::gc_malloc_allocator<String*>> m_intlAvailableLocales;
    Vector<String*, GCUtil::gc_malloc_allocator<String*>> m_intlCollatorAvailableLocales;
    Vector<String*, GCUtil::gc_malloc_allocator<String*>> m_intlPluralRulesAvailableLocales;
    Vector<String*, GCUtil::gc_malloc_allocator<String*>> m_caseMappingAvailableLocales;
#endif

#if defined(ENABLE_CODE_CACHE)
    CodeCache* m_codeCache;
#endif

#if defined(ENABLE_THREADING)
    Vector<AsyncWaiterDataItem, GCUtil::gc_malloc_allocator<AsyncWaiterDataItem>> m_asyncWaiterData;
    std::mutex m_asyncWaiterDataMutex;
    std::atomic_size_t m_pendingAsyncWaiterCount;

    std::condition_variable m_waitEventFromAnotherThreadConditionVariable;
#endif
};
} // namespace Escargot

#endif
