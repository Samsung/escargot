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

#include "runtime/Platform.h"
#include "runtime/Context.h"
#include "runtime/AtomicString.h"
#include "runtime/GlobalObject.h"
#include "runtime/RegExpObject.h"
#include "runtime/StaticStrings.h"
#include "runtime/String.h"
#include "runtime/Symbol.h"
#include "runtime/ToStringRecursionPreventer.h"

namespace Escargot {

class SandBox;
class CodeBlock;
class JobQueue;
class Job;
class ASTAllocator;
class CompressibleString;

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
    VMInstance(Platform* platform, const char* locale = nullptr, const char* timezone = nullptr);
    ~VMInstance();

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    void clearCaches();

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
    DateObject* cachedUTC() const
    {
        return m_cachedUTC;
    }

    void setCachedUTC(DateObject* d)
    {
        m_cachedUTC = d;
    }

    // object
    // []

    // undefined setter
    static bool undefinedNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // function
    // [name, length] or [prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // string
    // [length]
    static Value stringLengthNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static bool stringLengthNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

    // regexp
    // [lastIndex, source, flags, global, ignoreCase, multiline]
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData);
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpSourceNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpFlagsNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpGlobalNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpMultilineNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpStickyNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static Value regexpUnicodeNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);

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

    void enqueuePromiseJob(PromiseObject* promise, Job* job);
    bool hasPendingPromiseJob();
    SandBox::SandBoxResult executePendingPromiseJob();

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

    std::mt19937& randEngine()
    {
        return m_randEngine;
    }

    Platform* platform()
    {
        return m_platform;
    }

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

private:
    StaticStrings m_staticStrings;
    AtomicStringMap m_atomicStringMap;
    GlobalSymbols m_globalSymbols;
    GlobalSymbolRegistryVector m_globalSymbolRegistry;
    SandBox* m_currentSandBox;

    std::mt19937 m_randEngine;

    bool m_isFinalized;
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
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForSymbolObject;
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

    static void gcEventCallback(GC_EventType t, void* data);
    void (*m_onVMInstanceDestroy)(VMInstance* instance, void* data);
    void* m_onVMInstanceDestroyData;

    ToStringRecursionPreventer m_toStringRecursionPreventer;

    void* m_stackStartAddress;

    // regexp object data
    WTF::BumpPointerAllocator* m_bumpPointerAllocator;
    RegExpCacheMap* m_regexpCache;
    ASCIIString** m_regexpOptionStringCache;

// date object data
#ifdef ENABLE_ICU
    std::string m_locale;
    VZone* m_timezone;
    std::string m_timezoneID;
#endif
    DateObject* m_cachedUTC;

    Platform* m_platform;
    ASTAllocator* m_astAllocator;

    // promise job queue
    JobQueue* m_jobQueue;
};
} // namespace Escargot

#endif
