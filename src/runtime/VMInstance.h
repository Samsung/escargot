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
    F(replace)

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

public:
    VMInstance(Platform* platform, const char* locale = nullptr, const char* timezone = nullptr);
    ~VMInstance();

    void clearCaches();

    const GlobalSymbols& globalSymbols()
    {
        return m_globalSymbols;
    }

    GlobalSymbolRegistryVector& globalSymbolRegistry()
    {
        return m_globalSymbolRegistry;
    }

#ifdef ENABLE_ICU
    icu::Locale& locale()
    {
        return m_locale;
    }

    void setLocale(icu::Locale locale)
    {
        m_locale = locale;
    }

    icu::TimeZone* timezone()
    {
        if (m_timezone == nullptr) {
            ensureTimezone();
        }
        return m_timezone;
    }

    void ensureTimezone()
    {
        if (m_timezoneID == "") {
            icu::TimeZone* tz = icu::TimeZone::createDefault();
            ASSERT(tz != nullptr);
            tz->getID(m_timezoneID);
            m_timezone = tz;
        } else {
            tzset();
            m_timezone = (icu::TimeZone::createTimeZone(m_timezoneID));
        }
    }

    void setTimezoneID(icu::UnicodeString id)
    {
        m_timezoneID = id;
    }
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

    // array
    // [length]
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData);

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

    Vector<String*, GCUtil::gc_malloc_allocator<String*>>& parsedSourceCodes()
    {
        return m_parsedSourceCodes;
    }

    Vector<CodeBlock*, GCUtil::gc_malloc_allocator<CodeBlock*>>& compiledCodeBlocks()
    {
        return m_compiledCodeBlocks;
    }

    size_t& compiledByteCodeSize()
    {
        return m_compiledByteCodeSize;
    }

    std::mt19937& randEngine()
    {
        return m_randEngine;
    }

    Platform* platform()
    {
        return m_platform;
    }

private:
    StaticStrings m_staticStrings;
    AtomicStringMap m_atomicStringMap;
    GlobalSymbols m_globalSymbols;
    GlobalSymbolRegistryVector m_globalSymbolRegistry;
    Vector<SandBox*, GCUtil::gc_malloc_allocator<SandBox*>> m_sandBoxStack;

    std::mt19937 m_randEngine;

    // this flag should affect VM-wide array object
    bool m_didSomePrototypeObjectDefineIndexedProperty : 1;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForBoundFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForArrayObject;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForSymbolObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForMappedArgumentsObject;
    ObjectStructure* m_defaultStructureForUnmappedArgumentsObject;

    Vector<String*, GCUtil::gc_malloc_allocator<String*>> m_parsedSourceCodes;
    Vector<CodeBlock*, GCUtil::gc_malloc_allocator<CodeBlock*>> m_compiledCodeBlocks;
    size_t m_compiledByteCodeSize;

    ToStringRecursionPreventer m_toStringRecursionPreventer;

    // regexp object data
    WTF::BumpPointerAllocator* m_bumpPointerAllocator;
    RegExpCacheMap m_regexpCache;

// date object data
#ifdef ENABLE_ICU
    icu::Locale m_locale;
    icu::TimeZone* m_timezone;
    icu::UnicodeString m_timezoneID;
#endif
    DateObject* m_cachedUTC;

    Platform* m_platform;

    // promise job queue
    JobQueue* m_jobQueue;
};
} // namespace Escargot

#endif
