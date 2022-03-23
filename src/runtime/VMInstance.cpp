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

#include "Escargot.h"
#include "VMInstance.h"
#include "runtime/Global.h"
#include "runtime/ThreadLocal.h"
#include "runtime/Platform.h"
#include "runtime/ArrayObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/StringObject.h"
#include "runtime/DateObject.h"
#include "runtime/JobQueue.h"
#include "runtime/CompressibleString.h"
#include "runtime/ReloadableString.h"
#include "intl/Intl.h"
#include "interpreter/ByteCode.h"
#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCache.h"
#endif

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <intrin.h>
NT_TIB* getTIB()
{
#ifdef _M_IX86
    return (NT_TIB*)__readfsdword(0x18);
#elif _M_AMD64
    return (NT_TIB*)__readgsqword(0x30);
#else
#error unsupported architecture
#endif
}
#else
#include <pthread.h>
#endif

namespace Escargot {

#if defined(ENABLE_WASM)
#ifndef ESCARGOT_WASM_GC_CHECK_INTERVAL
#define ESCARGOT_WASM_GC_CHECK_INTERVAL 10000
#endif
#endif

Value VMInstance::functionPrototypeNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea)
{
    ASSERT(self->isFunctionObject());
    if (privateDataFromObjectPrivateArea.isEmpty()) {
        return self->asFunctionObject()->getFunctionPrototype(state);
    } else {
        return privateDataFromObjectPrivateArea;
    }
}

bool VMInstance::functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isFunctionObject());
    privateDataFromObjectPrivateArea = setterInputData;
    return true;
}

static ObjectPropertyNativeGetterSetterData functionPrototypeNativeGetterSetterData(
    true, false, false, &VMInstance::functionPrototypeNativeGetter, &VMInstance::functionPrototypeNativeSetter);

static ObjectPropertyNativeGetterSetterData builtinFunctionPrototypeNativeGetterSetterData(
    false, false, false, &VMInstance::functionPrototypeNativeGetter, &VMInstance::functionPrototypeNativeSetter);

Value VMInstance::stringLengthNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea)
{
    ASSERT(self->isStringObject());
    return Value(self->asStringObject()->primitiveValue()->length());
}

bool VMInstance::stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    return false;
}

static ObjectPropertyNativeGetterSetterData stringLengthGetterSetterData(
    false, false, false, &VMInstance::stringLengthNativeGetter, &VMInstance::stringLengthNativeSetter);

Value VMInstance::regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea)
{
    if (!self->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return self->asRegExpObject()->lastIndex();
}

bool VMInstance::regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isRegExpObject());
    self->asRegExpObject()->setLastIndex(state, setterInputData);
    return true;
}

static ObjectPropertyNativeGetterSetterData regexpLastIndexGetterSetterData(
    true, false, false, &VMInstance::regexpLastIndexNativeGetter, &VMInstance::regexpLastIndexNativeSetter);

#if defined(ENABLE_COMPRESSIBLE_STRING)

#ifndef ESCARGOT_COMPRESSIBLE_COMPRESS_GC_CHECK_INTERVAL
#define ESCARGOT_COMPRESSIBLE_COMPRESS_GC_CHECK_INTERVAL 1000
#endif
#ifndef ESCARGOT_COMPRESSIBLE_COMPRESS_USED_BEFORE_INTERVAL
#define ESCARGOT_COMPRESSIBLE_COMPRESS_USED_BEFORE_INTERVAL 1000
#endif
#ifndef ESCARGOT_COMPRESSIBLE_COMPRESS_MIN_SIZE
#define ESCARGOT_COMPRESSIBLE_COMPRESS_MIN_SIZE 1024 * 128
#endif

void VMInstance::compressStringsIfNeeds(uint64_t currentTickCount)
{
    auto& currentAllocatedCompressibleStrings = compressibleStrings();
    const size_t& currentAllocatedCompressibleStringsCount = currentAllocatedCompressibleStrings.size();
    size_t mostBigIndex = SIZE_MAX;

    for (size_t i = 0; i < currentAllocatedCompressibleStringsCount; i++) {
        if (!currentAllocatedCompressibleStrings[i]->isCompressed()
            && currentTickCount - currentAllocatedCompressibleStrings[i]->m_lastUsedTickcount > ESCARGOT_COMPRESSIBLE_COMPRESS_USED_BEFORE_INTERVAL
            && currentAllocatedCompressibleStrings[i]->decomressedBufferSize() > ESCARGOT_COMPRESSIBLE_COMPRESS_MIN_SIZE) {
            if (mostBigIndex == SIZE_MAX) {
                mostBigIndex = i;
            } else if (currentAllocatedCompressibleStrings[i]->decomressedBufferSize() > currentAllocatedCompressibleStrings[mostBigIndex]->decomressedBufferSize()) {
                mostBigIndex = i;
            }
        }
    }

    if (mostBigIndex != SIZE_MAX) {
        currentAllocatedCompressibleStrings[mostBigIndex]->compress();
    }
}
#endif

void* VMInstance::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(VMInstance)] = { 0 };
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_staticStrings.dtoaCache));

        // we should mark every word of m_atomicStringMap
        for (size_t i = 0; i < sizeof(m_atomicStringMap); i += sizeof(size_t)) {
            GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_atomicStringMap) + (i / sizeof(size_t)));
        }

#define DECLARE_GLOBAL_SYMBOLS(name) GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_globalSymbols.name));
        DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_globalSymbolRegistry));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_currentSandBox));

        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForFunctionObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForNotConstructorFunctionObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForBuiltinFunctionObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForFunctionPrototypeObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForBoundFunctionObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForClassConstructorFunctionObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForClassConstructorFunctionObjectWithName));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForStringObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForRegExpObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForMappedArgumentsObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForUnmappedArgumentsObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultPrivateMemberStructure));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_onVMInstanceDestroyData));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_toStringRecursionPreventer.m_registeredItems));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_regexpCache));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_regexpOptionStringCache));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_cachedUTC));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_jobQueue));
#if defined(ENABLE_INTL)
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_intlAvailableLocales));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_intlCollatorAvailableLocales));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_intlPluralRulesAvailableLocales));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_caseMappingAvailableLocales));
#endif

        descr = GC_make_descriptor(desc, GC_WORD_LEN(VMInstance));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void vmMarkStartCallback(void* data)
{
    VMInstance* self = (VMInstance*)data;

#if !defined(ESCARGOT_DEBUGGER)
    // in debugger mode, do not remove ByteCodeBlock
    if (self->m_regexpCache->size() > REGEXP_CACHE_SIZE_MAX || UNLIKELY(self->inIdleMode())) {
        self->m_regexpCache->clear();
    }

    auto& currentCodeSizeTotal = self->compiledByteCodeSize();
    if (currentCodeSizeTotal > SCRIPT_FUNCTION_OBJECT_BYTECODE_SIZE_MAX || UNLIKELY(self->inIdleMode())) {
        currentCodeSizeTotal = std::numeric_limits<size_t>::max();

        auto& v = self->compiledByteCodeBlocks();
        for (size_t i = 0; i < v.size(); i++) {
            auto cb = v[i]->m_codeBlock;
            if (LIKELY(!cb->isAsync() && !cb->isGenerator())) {
                v[i]->m_codeBlock->setByteCodeBlock(nullptr);
            }
        }
    }
#endif
}

void vmReclaimEndCallback(void* data)
{
    VMInstance* self = (VMInstance*)data;

#if defined(ENABLE_COMPRESSIBLE_STRING) || defined(ENABLE_WASM)
    auto currentTick = fastTickCount();
#if defined(ENABLE_COMPRESSIBLE_STRING)
    if (currentTick - self->m_lastCompressibleStringsTestTime > ESCARGOT_COMPRESSIBLE_COMPRESS_GC_CHECK_INTERVAL) {
        self->compressStringsIfNeeds(currentTick);
        self->m_lastCompressibleStringsTestTime = currentTick;
    }
#endif
#if defined(ENABLE_WASM)
    if (currentTick - ThreadLocal::wasmLastGCCheckTime() > ESCARGOT_WASM_GC_CHECK_INTERVAL) {
        ThreadLocal::wasmGC(currentTick);
    }
#endif
#endif

    auto& currentCodeSizeTotal = self->compiledByteCodeSize();

    if (currentCodeSizeTotal == std::numeric_limits<size_t>::max()) {
        GC_invoke_finalizers();

        // we need to check this because ~VMInstance can be called by GC_invoke_finalizers
        if (!self->m_isFinalized) {
            currentCodeSizeTotal = 0;
            auto& v = self->compiledByteCodeBlocks();
            for (size_t i = 0; i < v.size(); i++) {
                auto cb = v[i]->m_codeBlock;
                if (UNLIKELY(!cb->isAsync() && !cb->isGenerator())) {
                    v[i]->m_codeBlock->setByteCodeBlock(v[i]);
                }
                ASSERT(v[i]->m_codeBlock->byteCodeBlock() == v[i]);

                currentCodeSizeTotal += v[i]->memoryAllocatedSize();
            }
        }
    }

    /*
    if (t == GC_EventType::GC_EVENT_RECLAIM_END) {
        printf("Done GC: HeapSize: [%f MB , %f MB]\n", GC_get_memory_use() / 1024.f / 1024.f, GC_get_heap_size() / 1024.f / 1024.f);
        printf("bytecode Size %f KiB codeblock count %zu\n", self->compiledByteCodeSize() / 1024.f, self->m_compiledByteCodeBlocks.size());
        printf("regexp cache size %zu\n", self->regexpCache()->size());
    }
    */
}

VMInstance::~VMInstance()
{
    {
        auto& v = compiledByteCodeBlocks();
        for (size_t i = 0; i < v.size(); i++) {
            v[i]->m_isOwnerMayFreed = true;
        }
    }
#if defined(ENABLE_COMPRESSIBLE_STRING)
    {
        auto& v = compressibleStrings();
        for (size_t i = 0; i < v.size(); i++) {
            v[i]->m_isOwnerMayFreed = true;
        }
    }
#endif
#if defined(ENABLE_RELOADABLE_STRING)
    {
        auto& v = reloadableStrings();
        for (size_t i = 0; i < v.size(); i++) {
            v[i]->m_isOwnerMayFreed = true;
        }
    }
#endif

    m_isFinalized = true;

    // remove gc event callback
    {
        GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
        Optional<GCEventListenerSet::EventListenerVector*> msListeners = list.markStartListeners();
        ASSERT(msListeners.hasValue());
        auto iter = std::find(msListeners->begin(), msListeners->end(), std::make_pair(vmMarkStartCallback, static_cast<void*>(this)));
        ASSERT(iter != msListeners->end());
        msListeners->erase(iter);

        Optional<GCEventListenerSet::EventListenerVector*> reListeners = list.reclaimEndListeners();
        ASSERT(reListeners.hasValue());
        iter = std::find(reListeners->begin(), reListeners->end(), std::make_pair(vmReclaimEndCallback, static_cast<void*>(this)));
        ASSERT(iter != reListeners->end());
        reListeners->erase(iter);
    }

    if (m_onVMInstanceDestroy) {
        m_onVMInstanceDestroy(this, m_onVMInstanceDestroyData);
    }

    clearCachesRelatedWithContext();
#if defined(ENABLE_ICU) && !defined(OS_WINDOWS_UWP)
    vzone_close(m_timezone);
#endif

#if defined(ENABLE_CODE_CACHE)
    delete m_codeCache;
#endif
}

VMInstance::VMInstance(const char* locale, const char* timezone, const char* baseCacheDir)
    : m_staticStrings(&m_atomicStringMap)
    , m_currentSandBox(nullptr)
    , m_isFinalized(false)
    , m_inIdleMode(false)
    , m_didSomePrototypeObjectDefineIndexedProperty(false)
    , m_compiledByteCodeSize(0)
#if defined(ENABLE_COMPRESSIBLE_STRING)
    , m_lastCompressibleStringsTestTime(0)
    , m_compressibleStringsUncomressedBufferSize(0)
#endif
    , m_onVMInstanceDestroy(nullptr)
    , m_onVMInstanceDestroyData(nullptr)
    , m_errorCreationCallback(nullptr)
    , m_errorCreationCallbackPublic(nullptr)
    , m_promiseHook(nullptr)
    , m_promiseHookPublic(nullptr)
    , m_promiseRejectCallback(nullptr)
    , m_promiseRejectCallbackPublic(nullptr)
    , m_cachedUTC(nullptr)
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        VMInstance* self = (VMInstance*)obj;
        self->~VMInstance();
    },
                                   nullptr, nullptr, nullptr);

#if defined(OS_WINDOWS)
    m_stackStartAddress = getTIB()->StackBase;
#else
    pthread_attr_t attr;
    RELEASE_ASSERT(pthread_getattr_np(pthread_self(), &attr) == 0);

    size_t size;
    RELEASE_ASSERT(pthread_attr_getstack(&attr, &m_stackStartAddress, &size) == 0);
    pthread_attr_destroy(&attr);
#ifdef STACK_GROWS_DOWN
    m_stackStartAddress = (char*)m_stackStartAddress + size;
#endif
#endif
    // test stack base property aligned
    RELEASE_ASSERT(((size_t)m_stackStartAddress) % sizeof(size_t) == 0);

    if (!String::emptyString) {
        String::emptyString = new (NoGC) ASCIIString("");
    }
    m_staticStrings.initStaticStrings();

    m_regexpCache = new (GC) RegExpCacheMap();
    m_regexpOptionStringCache = (ASCIIString**)GC_MALLOC(64 * sizeof(ASCIIString*));
    memset(m_regexpOptionStringCache, 0, 64 * sizeof(ASCIIString*));

#if defined(ENABLE_ICU)
#if !defined(OS_WINDOWS_UWP)
    m_timezone = nullptr;
#endif
    if (timezone) {
        m_timezoneID = timezone;
    } else if (getenv("TZ")) {
        m_timezoneID = getenv("TZ");
    } else {
        m_timezoneID = "";
    }

    if (locale) {
        m_locale = locale;
    } else if (getenv("LOCALE") && strlen(getenv("LOCALE"))) {
        m_locale = getenv("LOCALE");
    } else {
#if defined(ENABLE_RUNTIME_ICU_BINDER)
        m_locale = RuntimeICUBinder::ICU::findSystemLocale();
#else
        m_locale = uloc_getDefault();
#endif
    }
#endif

    // add gc event callback
    GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
    list.ensureMarkStartListeners()->push_back(std::make_pair(vmMarkStartCallback, this));
    list.ensureReclaimEndListeners()->push_back(std::make_pair(vmReclaimEndCallback, this));

#define DECLARE_GLOBAL_SYMBOLS(name) m_globalSymbols.name = new Symbol(new ASCIIStringFromExternalMemory("Symbol." #name, sizeof("Symbol." #name) - 1));
    DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

    ExecutionState stateForInit((Context*)nullptr);

    m_defaultStructureForObject = new ObjectStructureWithTransition(ObjectStructureItemTightVector(), false, false, false, false);

    m_defaultStructureForFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.prototype,
                                                                                   ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&functionPrototypeNativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(m_staticStrings.length,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(m_staticStrings.name,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.prototype,
                                                                                          ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(m_staticStrings.length,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));


    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(m_staticStrings.name,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.length,
                                                                                                 ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));


    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForNotConstructorFunctionObject->addProperty(m_staticStrings.name,
                                                                                                                       ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));


    m_defaultStructureForBoundFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.length,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));
    m_defaultStructureForBoundFunctionObject = m_defaultStructureForBoundFunctionObject->addProperty(m_staticStrings.name,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionPrototypeObject = m_defaultStructureForObject->addProperty(m_staticStrings.constructor,
                                                                                            ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForClassConstructorFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.length,
                                                                                                   ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForClassConstructorFunctionObject = m_defaultStructureForClassConstructorFunctionObject->addProperty(m_staticStrings.prototype,
                                                                                                                           ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));

    m_defaultStructureForClassConstructorFunctionObjectWithName = m_defaultStructureForObject->addProperty(m_staticStrings.length,
                                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForClassConstructorFunctionObjectWithName = m_defaultStructureForClassConstructorFunctionObjectWithName->addProperty(m_staticStrings.name,
                                                                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForClassConstructorFunctionObjectWithName = m_defaultStructureForClassConstructorFunctionObjectWithName->addProperty(m_staticStrings.prototype,
                                                                                                                                           ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));


    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(m_staticStrings.lastIndex,
                                                                                 ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));

    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(ObjectStructurePropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(m_staticStrings.callee, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(ObjectStructurePropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(m_staticStrings.callee, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));

    m_defaultPrivateMemberStructure = new ObjectPrivateMemberStructure();

    m_jobQueue = new JobQueue();

#if defined(ENABLE_CODE_CACHE)
    if (UNLIKELY(!baseCacheDir || strlen(baseCacheDir) == 0)) {
        const char* homeDir = getenv("HOME");
        baseCacheDir = (!homeDir || strlen(homeDir) == 0) ? "/tmp" : homeDir;
    }
    m_codeCache = new CodeCache(baseCacheDir);
#endif
}

#if defined(ENABLE_ICU)

#if defined(ENABLE_RUNTIME_ICU_BINDER)
static std::string findTimezone()
{
    return RuntimeICUBinder::ICU::findSystemTimezoneName();
}
#elif defined(OS_WINDOWS_UWP)
static std::string findTimezone()
{
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    DWORD ret = GetDynamicTimeZoneInformation(&tz);
    UErrorCode status = U_ZERO_ERROR;
    UChar result[256];
    int32_t len = ucal_getTimeZoneIDForWindowsID(
        (const UChar*)tz.TimeZoneKeyName, -1,
        "en_US",
        result,
        sizeof(result) / sizeof(UChar),
        &status);

    RELEASE_ASSERT(status == U_ZERO_ERROR);
    return (new UTF16String(result, len))->toNonGCUTF8StringData();
}
#else
static std::string findTimezone()
{
    auto tz = icu::TimeZone::detectHostTimeZone();
    icu::UnicodeString id;
    tz->getID(id);
    delete tz;
    std::string r;
    id.toUTF8String(r);
    return r;
}
#endif


void VMInstance::ensureTimezone()
{
    if (m_timezoneID == "") {
        m_timezoneID = findTimezone();
    } else {
#if !defined(OS_WINDOWS_UWP)
        tzset();
#endif
    }

#if !defined(OS_WINDOWS_UWP)
    auto u16 = utf8StringToUTF16String(m_timezoneID.data(), m_timezoneID.size());
    m_timezone = vzone_openID(u16.data(), u16.size());
#endif
}
#endif

DateObject* VMInstance::cachedUTC(ExecutionState& state)
{
    if (m_cachedUTC == nullptr) {
        DateObject* obj = new DateObject(state);
        obj->setPrototype(state, Value(Value::Null));
        m_cachedUTC = obj;
    }
    return m_cachedUTC;
}

void VMInstance::clearCachesRelatedWithContext()
{
    m_regexpCache->clear();
    globalSymbolRegistry().clear();
#if defined(ENABLE_CODE_CACHE)
    // CodeCache should be cleared here because CodeCache holds a lock of cache directory
    // this lock should be released immediately (destructor may be called later)
    m_codeCache->clear();
#endif
    bf_clear_cache(ThreadLocal::bfContext());
}

void VMInstance::enterIdleMode()
{
    m_inIdleMode = true;

    // user can call this function many times without many performance concern
    if (GC_get_bytes_since_gc() > 4096) {
        GC_gcollect_and_unmap();
        GC_gcollect_and_unmap();
        GC_gcollect_and_unmap();
    }

#if defined(ENABLE_COMPRESSIBLE_STRING)
    // ESCARGOT_LOG_INFO("compressibleStringsUncomressedBufferSize before %lfKB\n", m_compressibleStringsUncomressedBufferSize/1024.f);
    auto& currentAllocatedCompressibleStrings = compressibleStrings();
    const size_t& currentAllocatedCompressibleStringsCount = currentAllocatedCompressibleStrings.size();

    for (size_t i = 0; i < currentAllocatedCompressibleStringsCount; i++) {
        if (!currentAllocatedCompressibleStrings[i]->isCompressed()) {
            currentAllocatedCompressibleStrings[i]->compress();
        }
    }
// ESCARGOT_LOG_INFO("compressibleStringsUncomressedBufferSize after %lfKB\n", m_compressibleStringsUncomressedBufferSize/1024.f);
#endif

#if defined(ENABLE_RELOADABLE_STRING)
    auto& currentAllocatedReloadableStrings = reloadableStrings();
    const size_t& currentAllocatedReloadableStringsCount = currentAllocatedReloadableStrings.size();

    for (size_t i = 0; i < currentAllocatedReloadableStringsCount; i++) {
        if (!currentAllocatedReloadableStrings[i]->isUnloaded()) {
            currentAllocatedReloadableStrings[i]->unload();
        }
    }
#endif

    m_inIdleMode = false;
}

void VMInstance::somePrototypeObjectDefineIndexedProperty(ExecutionState& state)
{
    m_didSomePrototypeObjectDefineIndexedProperty = true;
    std::vector<ArrayObject*> allOfArray;
    Escargot::HeapObjectIteratorCallback callback =
        [&allOfArray](Escargot::ExecutionState& state, void* obj) {
            Escargot::ArrayObject* arr = (Escargot::ArrayObject*)obj;
            allOfArray.push_back(arr);
        };
    Escargot::ArrayObject::iterateArrays(state, callback);

    GC_disable();
    Vector<ArrayObject*, GCUtil::gc_malloc_allocator<ArrayObject*>> allOfArrayRooted;
    allOfArrayRooted.assign(allOfArray.data(), allOfArray.data() + allOfArray.size());
    GC_enable();

    for (size_t i = 0; i < allOfArrayRooted.size(); i++) {
        allOfArrayRooted[i]->convertIntoNonFastMode(state);
    }
}

void VMInstance::enqueueJob(Job* job)
{
    m_jobQueue->enqueueJob(job);
    Global::platform()->markJSJobEnqueued(job->relatedContext());
}

bool VMInstance::hasPendingJob()
{
    return m_jobQueue->hasNextJob();
}

SandBox::SandBoxResult VMInstance::executePendingJob()
{
    return m_jobQueue->nextJob()->run();
}

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
// some locale have script value on it eg) zh_Hant_HK. so we need to remove it
static std::string icuLocaleToBCP47LanguageRegionPair(const char* l)
{
    std::string v = l;
    for (size_t i = 0; i < v.length(); i++) {
        if (v[i] == '_') {
            v[i] = '-';
        }
    }
    auto c = Intl::canonicalizeLanguageTag(v);
    if (c.region.length()) {
        return c.language + "-" + c.region;
    }
    return c.language;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlCollatorAvailableLocales()
{
    if (m_intlCollatorAvailableLocales.size() == 0) {
        auto count = ucol_countAvailable();

        // after removing script value from locale
        // we should consider duplication
        // eg) zh, zh_Hans
        std::set<std::string> duplicateRemover;
        for (int32_t i = 0; i < count; ++i) {
            auto s = icuLocaleToBCP47LanguageRegionPair(ucol_getAvailable(i));
            if (duplicateRemover.find(s) == duplicateRemover.end()) {
                duplicateRemover.insert(s);
                String* locale = String::fromASCII(s.data(), s.length());
                m_intlCollatorAvailableLocales.pushBack(locale);
            }
        }
    }
    return m_intlCollatorAvailableLocales;
}

void VMInstance::ensureIntlSupportedLocales()
{
    if (m_intlAvailableLocales.size() == 0) {
#if !defined(NDEBUG)
        ASSERT(uloc_countAvailable() == udat_countAvailable());
        ASSERT(uloc_countAvailable() == unum_countAvailable());
        for (int32_t i = 0; i < uloc_countAvailable(); ++i) {
            ASSERT(std::string(uloc_getAvailable(i)) == std::string(udat_getAvailable(i)));
            ASSERT(std::string(uloc_getAvailable(i)) == std::string(unum_getAvailable(i)));
        }
#endif
        // after removing script value from locale
        // we should consider duplication
        // eg) zh, zh_Hans
        std::set<std::string> duplicateRemover;

        auto count = uloc_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            auto s = icuLocaleToBCP47LanguageRegionPair(uloc_getAvailable(i));
            if (duplicateRemover.find(s) == duplicateRemover.end()) {
                duplicateRemover.insert(s);
                String* locale = String::fromASCII(s.data(), s.length());
                m_intlAvailableLocales.pushBack(locale);
            }
        }
    }
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlDateTimeFormatAvailableLocales()
{
    ensureIntlSupportedLocales();
    return m_intlAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlNumberFormatAvailableLocales()
{
    ensureIntlSupportedLocales();
    return m_intlAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlRelativeTimeFormatAvailableLocales()
{
    ensureIntlSupportedLocales();
    return m_intlAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlDisplayNamesAvailableLocales()
{
    ensureIntlSupportedLocales();
    return m_intlAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlListFormatAvailableLocales()
{
    ensureIntlSupportedLocales();
    return m_intlAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::caseMappingAvailableLocales()
{
    if (m_caseMappingAvailableLocales.size() == 0) {
        m_caseMappingAvailableLocales.pushBack(String::fromASCII("tr"));
        m_caseMappingAvailableLocales.pushBack(String::fromASCII("el"));
        m_caseMappingAvailableLocales.pushBack(String::fromASCII("lt"));
        m_caseMappingAvailableLocales.pushBack(String::fromASCII("az"));
    }
    return m_caseMappingAvailableLocales;
}

const Vector<String*, GCUtil::gc_malloc_allocator<String*>>& VMInstance::intlPluralRulesAvailableLocales()
{
    if (m_intlPluralRulesAvailableLocales.size() == 0) {
        UErrorCode status = U_ZERO_ERROR;
        UResourceBundle* rb = ures_openDirect(nullptr, "plurals", &status);
        ASSERT(U_SUCCESS(status));
        UResourceBundle* locales = ures_getByKey(rb, "locales", nullptr, &status);
        ASSERT(U_SUCCESS(status));

        UResourceBundle* res = nullptr;

        std::set<std::string> duplicateRemover;

        while (true) {
            res = ures_getNextResource(locales, res, &status);
            if (res == nullptr || U_FAILURE(status)) {
                break;
            }
            auto s = icuLocaleToBCP47LanguageRegionPair(ures_getKey(res));
            if (duplicateRemover.find(s) == duplicateRemover.end()) {
                duplicateRemover.insert(s);
                String* locale = String::fromASCII(s.data(), s.length());
                m_intlPluralRulesAvailableLocales.pushBack(locale);
            }
        }

        ures_close(res);
        ures_close(locales);
        ures_close(rb);
    }
    return m_intlPluralRulesAvailableLocales;
}
#endif
} // namespace Escargot
