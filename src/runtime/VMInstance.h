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
    Optional<String*> key;
    Symbol* symbol;
};

typedef Vector<GlobalSymbolRegistryItem, GCUtil::gc_malloc_allocator<GlobalSymbolRegistryItem>> GlobalSymbolRegistryVector;

class VMInstance : public gc {
    friend class Context;
    friend class VMInstanceRef;
    friend class ScriptParser;
    friend class SandBox;

public:
    enum PromiseHookType {
        Init,
        Resolve,
        Before,
        After
    };

    typedef void (*PromiseHook)(ExecutionState& state, PromiseHookType type, PromiseObject* promise, const Value& parent, void* hook);

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

    const std::string& timezoneID()
    {
        return m_timezoneID;
    }

    VZone* timezone()
    {
        if (m_timezone == nullptr) {
            ensureTimezone();
        }
        return m_timezone;
    }

    void ensureTimezone();
#endif
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

    bool didSomePrototypeObjectDefineIndexedProperty()
    {
        return m_didSomePrototypeObjectDefineIndexedProperty;
    }

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

    std::vector<ByteCodeBlock*>& compiledByteCodeBlocks()
    {
        return m_compiledByteCodeBlocks;
    }

    size_t& compiledByteCodeSize()
    {
        return m_compiledByteCodeSize;
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

    void* stackStartAddress()
    {
        return m_stackStartAddress;
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

    void registerErrorCreationCallback(void (*ErrorCreationCallback)(ExecutionState& state, ErrorObject* err, void* cb), void* callbackPublic)
    {
        m_errorCreationCallback = ErrorCreationCallback;
        m_errorCreationCallbackPublic = callbackPublic;
    }

    void unregisterErrorCreationCallback()
    {
        m_errorCreationCallback = nullptr;
        m_errorCreationCallbackPublic = nullptr;
    }

    void triggerErrorCreationCallback(ExecutionState& state, ErrorObject* error)
    {
        ASSERT(!!m_errorCreationCallback);
        if (m_errorCreationCallbackPublic) {
            m_errorCreationCallback(state, error, m_errorCreationCallbackPublic);
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

private:
    StaticStrings m_staticStrings;
    AtomicStringMap m_atomicStringMap;
    GlobalSymbols m_globalSymbols;
    GlobalSymbolRegistryVector m_globalSymbolRegistry;
    SandBox* m_currentSandBox;

    bool m_isFinalized;
    bool m_inEnterIdleMode;
    // this flag should affect VM-wide array object
    bool m_didSomePrototypeObjectDefineIndexedProperty;
#ifdef ESCARGOT_DEBUGGER
    bool m_debuggerEnabled;
#endif /* ESCARGOT_DEBUGGER */

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

    std::vector<ByteCodeBlock*> m_compiledByteCodeBlocks;
    size_t m_compiledByteCodeSize;

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

    // PromiseHook is triggered for each Promise event
    // Third party app registers PromiseHook when it is necessary
    PromiseHook m_promiseHook;
    void* m_promiseHookPublic;

    ToStringRecursionPreventer m_toStringRecursionPreventer;

    void* m_stackStartAddress;

    // regexp object data
    RegExpCacheMap* m_regexpCache;
    ASCIIString** m_regexpOptionStringCache;

// date object data
#ifdef ENABLE_ICU
    std::string m_locale;
    VZone* m_timezone;
    std::string m_timezoneID;
#endif
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
};
} // namespace Escargot

#endif
