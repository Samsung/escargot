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
#include "runtime/RegExpObject.h"

#if defined(ENABLE_WASM)
struct wasm_engine_t;
struct wasm_store_t;
struct WASMContext {
    wasm_engine_t* engine;
    wasm_store_t* store;
};
#endif

namespace WTF {
class BumpPointerAllocator;
}

namespace Escargot {

class Context;
class CodeBlock;
class JobQueue;
class Job;
class ASTAllocator;
class Symbol;
class String;
class Platform;
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

    /////////////////////////////////
    // Global Data
    // global values which should be initialized once and shared during the runtime
    static Platform* g_platform;

    static std::mt19937 g_randEngine;
    static bf_context_t g_bfContext;
#if defined(ENABLE_WASM)
    static WASMContext g_wasmContext;
#endif

    static ASTAllocator* g_astAllocator;
    static WTF::BumpPointerAllocator* g_bumpPointerAllocator;
    /////////////////////////////////

public:
    /////////////////////////////////
    // Global Data Static Function
    static void initialize(Platform* platform);
    static void finalize();
    static Platform* platform()
    {
        ASSERT(!!g_platform);
        return g_platform;
    }
    static std::mt19937& randEngine()
    {
        return g_randEngine;
    }
    static bf_context_t* bfContext()
    {
        ASSERT(!!g_bfContext.realloc_func);
        return &g_bfContext;
    }
#if defined(ENABLE_WASM)
    static wasm_store_t* wasmStore()
    {
        ASSERT(!!g_wasmContext.store);
        return g_wasmContext.store;
    }
#endif
    static ASTAllocator* astAllocator()
    {
        ASSERT(!!g_astAllocator);
        return g_astAllocator;
    }
    static WTF::BumpPointerAllocator* bumpPointerAllocator()
    {
        ASSERT(!!g_bumpPointerAllocator);
        return g_bumpPointerAllocator;
    }
    /////////////////////////////////

    VMInstance(const char* locale = nullptr, const char* timezone = nullptr);
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

    // undefined setter
    static bool undefinedNativeSetter(ExecutionState& state, Object* self, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // function
    // [name, length] or [prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // string
    // [length]
    static Value stringLengthNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static bool stringLengthNativeSetter(ExecutionState& state, Object* self, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // regexp
    // [lastIndex, source, flags, global, ignoreCase, multiline]
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData);
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpSourceNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpFlagsNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpGlobalNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpMultilineNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpStickyNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);
    static Value regexpUnicodeNativeGetter(ExecutionState& state, Object* self, const EncodedValue& privateDataFromObjectPrivateArea);

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

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlCollatorAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlDateTimeFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlNumberFormatAvailableLocales();
    const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& intlRelativeTimeFormatAvailableLocales();
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
