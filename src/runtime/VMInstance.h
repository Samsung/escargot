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

#ifndef __EscargotVMInstance__
#define __EscargotVMInstance__

#include "runtime/Context.h"
#include "runtime/AtomicString.h"
#include "runtime/GlobalObject.h"
#include "runtime/RegExpObject.h"
#include "runtime/StaticStrings.h"
#include "runtime/String.h"
#include "runtime/ToStringRecursionPreventer.h"

namespace Escargot {

class SandBox;
class CodeBlock;
class JobQueue;

class VMInstance : public gc {
    friend class Context;

public:
    VMInstance();
#ifdef ENABLE_ICU
    icu::Locale& locale()
    {
        return m_locale;
    }

    // The function below will be used by StarFish
    void setLocale(icu::Locale locale)
    {
        m_locale = locale;
    }

    icu::TimeZone* timezone() const
    {
        return m_timezone;
    }

    void setTimezone()
    {
        if (m_timezoneID == "") {
            icu::TimeZone* tz = icu::TimeZone::createDefault();
            ASSERT(tz != nullptr);
            tz->getID(m_timezoneID);
            delete tz;
        }
        m_timezone = (icu::TimeZone::createTimeZone(m_timezoneID))->clone();
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

    // function
    // [name, length] or [prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // array
    // [length]
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // string
    // [length]
    static Value stringLengthNativeGetter(ExecutionState& state, Object* self);
    static bool stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // regexp
    // [lastIndex, source, global, ignoreCase, multiline]
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self);
    static Value regexpSourceNativeGetter(ExecutionState& state, Object* self);
    static Value regexpGlobalNativeGetter(ExecutionState& state, Object* self);
    static Value regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self);
    static Value regexpMultilineNativeGetter(ExecutionState& state, Object* self);

    bool didSomePrototypeObjectDefineIndexedProperty()
    {
        return m_didSomePrototypeObjectDefineIndexedProperty;
    }

    void somePrototypeObjectDefineIndexedProperty(ExecutionState& state);

    ToStringRecursionPreventer& toStringRecursionPreventer()
    {
        return m_toStringRecursionPreventer;
    }

protected:
    StaticStrings m_staticStrings;
    AtomicStringMap m_atomicStringMap;
    Vector<SandBox*, GCUtil::gc_malloc_allocator<SandBox*>> m_sandBoxStack;

    // this flag should affect VM-wide array object
    bool m_didSomePrototypeObjectDefineIndexedProperty;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionObjectInStrictMode;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObjectInStrictMode;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForArrayObject;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForArgumentsObject;
    ObjectStructure* m_defaultStructureForArgumentsObjectInStrictMode;

    Vector<CodeBlock*, GCUtil::gc_malloc_ignore_off_page_allocator<CodeBlock*>> m_compiledCodeBlocks;

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

// promise data
#if ESCARGOT_ENABLE_PROMISE
    JobQueue* m_jobQueue;
#endif
};
}

#endif
