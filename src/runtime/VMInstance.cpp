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

#include "Escargot.h"
#include "VMInstance.h"
#include "BumpPointerAllocator.h"
#include "runtime/ArrayObject.h"
#include "runtime/SharedArrayBufferObject.h"
#include "runtime/StringObject.h"
#include "runtime/JobQueue.h"
#include "runtime/CompressibleString.h"
#include "interpreter/ByteCode.h"
#include "parser/ASTAllocator.h"

#include <pthread.h>

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_objectRareDataTag;
extern size_t g_symbolTag;

bool VMInstance::undefinedNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    return false;
}

Value VMInstance::functionPrototypeNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    ASSERT(self->isFunctionObject());
    if (privateDataFromObjectPrivateArea.isEmpty()) {
        return self->asFunctionObject()->getFunctionPrototype(state);
    } else {
        return privateDataFromObjectPrivateArea;
    }
}

bool VMInstance::functionPrototypeNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isFunctionObject());
    privateDataFromObjectPrivateArea = setterInputData;
    return true;
}

static ObjectPropertyNativeGetterSetterData functionPrototypeNativeGetterSetterData(
    true, false, false, &VMInstance::functionPrototypeNativeGetter, &VMInstance::functionPrototypeNativeSetter);

static ObjectPropertyNativeGetterSetterData builtinFunctionPrototypeNativeGetterSetterData(
    false, false, false, &VMInstance::functionPrototypeNativeGetter, &VMInstance::functionPrototypeNativeSetter);

Value VMInstance::stringLengthNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    ASSERT(self->isStringObject());
    return Value(self->asStringObject()->primitiveValue()->length());
}

bool VMInstance::stringLengthNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    return false;
}

static ObjectPropertyNativeGetterSetterData stringLengthGetterSetterData(
    false, false, false, &VMInstance::stringLengthNativeGetter, &VMInstance::stringLengthNativeSetter);

Value VMInstance::regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (!self->isRegExpObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return self->asRegExpObject()->lastIndex();
}

bool VMInstance::regexpLastIndexNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isRegExpObject());
    self->asRegExpObject()->setLastIndex(state, setterInputData);
    return true;
}

static ObjectPropertyNativeGetterSetterData regexpLastIndexGetterSetterData(
    true, false, false, &VMInstance::regexpLastIndexNativeGetter, &VMInstance::regexpLastIndexNativeSetter);
#if defined(ENABLE_COMPRESSIBLE_STRING)

#define COMPRESSIBLE_COMPRESS_CHECK_INTERVAL 1000
#define COMPRESSIBLE_COMPRESS_USED_BEFORE_INTERVAL 1000
#define COMPRESSIBLE_COMPRESS_MIN_SIZE 1024 * 128

void VMInstance::compressStringsIfNeeds(uint64_t currentTickCount)
{
    auto& currentAllocatedCompressibleStrings = compressibleStrings();
    const size_t& currentAllocatedCompressibleStringsCount = currentAllocatedCompressibleStrings.size();
    size_t mostBigIndex = SIZE_MAX;

    for (size_t i = 0; i < currentAllocatedCompressibleStringsCount; i++) {
        if (!currentAllocatedCompressibleStrings[i]->isCompressed() && currentTickCount - currentAllocatedCompressibleStrings[i]->m_lastUsedTickcount > COMPRESSIBLE_COMPRESS_USED_BEFORE_INTERVAL && currentAllocatedCompressibleStrings[i]->decomressedBufferSize() > COMPRESSIBLE_COMPRESS_MIN_SIZE) {
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

void VMInstance::gcEventCallback(GC_EventType t, void* data)
{
    VMInstance* self = (VMInstance*)data;
    if (t == GC_EventType::GC_EVENT_MARK_START) {
        if (self->m_regexpCache->size() > REGEXP_CACHE_SIZE_MAX) {
            self->m_regexpCache->clear();
        }

        auto& currentCodeSizeTotal = self->compiledByteCodeSize();
        if (currentCodeSizeTotal > SCRIPT_FUNCTION_OBJECT_BYTECODE_SIZE_MAX) {
            currentCodeSizeTotal = std::numeric_limits<size_t>::max();
            auto& v = self->compiledByteCodeBlocks();
            for (size_t i = 0; i < v.size(); i++) {
                auto cb = v[i]->m_codeBlock;
                v[i]->m_codeBlock->m_byteCodeBlock = nullptr;
            }
        }
    } else if (t == GC_EventType::GC_EVENT_RECLAIM_END) {
#if defined(ENABLE_COMPRESSIBLE_STRING)
        auto currentTick = fastTickCount();
        if (currentTick - self->m_lastCompressibleStringsTestTime > COMPRESSIBLE_COMPRESS_CHECK_INTERVAL) {
            self->compressStringsIfNeeds(currentTick);
            self->m_lastCompressibleStringsTestTime = currentTick;
        }
#endif
        auto& currentCodeSizeTotal = self->compiledByteCodeSize();

        if (currentCodeSizeTotal == std::numeric_limits<size_t>::max()) {
            GC_invoke_finalizers();

            // we need to check this because ~VMInstance can be called by GC_invoke_finalizers
            if (!self->m_isFinalized) {
                currentCodeSizeTotal = 0;
                auto& v = self->compiledByteCodeBlocks();
                for (size_t i = 0; i < v.size(); i++) {
                    v[i]->m_codeBlock->m_byteCodeBlock = v[i];
                    currentCodeSizeTotal += v[i]->memoryAllocatedSize();
                }
            }
        }
    }
    /*
    if (t == GC_EventType::GC_EVENT_RECLAIM_END) {
        printf("Done GC: HeapSize: [%f MB , %f MB]\n", GC_get_memory_use() / 1024.f / 1024.f, GC_get_heap_size() / 1024.f / 1024.f);
        printf("bytecode Size %f KiB codeblock count %zu\n", self->compiledByteCodeSize() / 1024.f, self->m_compiledByteCodeBlocks.size());
        printf("regexp cache size %zu\n", self->m_regexpCache->size());
    }
    */
}

void* VMInstance::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
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
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForStringObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForSymbolObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForRegExpObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForMappedArgumentsObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_defaultStructureForUnmappedArgumentsObject));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_onVMInstanceDestroyData));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_toStringRecursionPreventer.m_registeredItems));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_bumpPointerAllocator));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_regexpCache));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_regexpOptionStringCache));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_cachedUTC));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_platform));
        GC_set_bit(desc, GC_WORD_OFFSET(VMInstance, m_jobQueue));

        descr = GC_make_descriptor(desc, GC_WORD_LEN(VMInstance));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
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
    m_isFinalized = true;
    GC_remove_event_callback(gcEventCallback, this);
    if (m_onVMInstanceDestroy) {
        m_onVMInstanceDestroy(this, m_onVMInstanceDestroyData);
    }
    clearCaches();
#ifdef ENABLE_ICU
    vzone_close(m_timezone);
#endif
    delete m_astAllocator;
}

VMInstance::VMInstance(Platform* platform, const char* locale, const char* timezone)
    : m_currentSandBox(nullptr)
    , m_randEngine((unsigned int)time(NULL))
    , m_isFinalized(false)
    , m_didSomePrototypeObjectDefineIndexedProperty(false)
    , m_compiledByteCodeSize(0)
#if defined(ENABLE_COMPRESSIBLE_STRING)
    , m_lastCompressibleStringsTestTime(0)
    , m_compressibleStringsUncomressedBufferSize(0)
#endif
    , m_onVMInstanceDestroy(nullptr)
    , m_onVMInstanceDestroyData(nullptr)
    , m_cachedUTC(nullptr)
    , m_platform(platform)
    , m_astAllocator(new ASTAllocator())
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        VMInstance* self = (VMInstance*)obj;
        self->~VMInstance();
    },
                                   nullptr, nullptr, nullptr);


    pthread_attr_t attr;
    RELEASE_ASSERT(pthread_getattr_np(pthread_self(), &attr) == 0);

    size_t size;
    RELEASE_ASSERT(pthread_attr_getstack(&attr, &m_stackStartAddress, &size) == 0);
    pthread_attr_destroy(&attr);
#ifdef STACK_GROWS_DOWN
    m_stackStartAddress = (char*)m_stackStartAddress + size;
#endif

    // test stack base property aligned
    RELEASE_ASSERT(((size_t)m_stackStartAddress) % sizeof(size_t) == 0);

    if (!String::emptyString) {
        String::emptyString = new (NoGC) ASCIIString("");
    }
    m_staticStrings.initStaticStrings(&m_atomicStringMap);

    m_bumpPointerAllocator = new (PointerFreeGC) WTF::BumpPointerAllocator();
    m_regexpCache = new (GC) RegExpCacheMap();
    m_regexpOptionStringCache = (ASCIIString**)GC_MALLOC(64 * sizeof(ASCIIString*));
    memset(m_regexpOptionStringCache, 0, 64 * sizeof(ASCIIString*));

#ifdef ENABLE_ICU
    m_timezone = nullptr;
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

#if defined(ESCARGOT_ENABLE_TEST)
    if (getenv("RANDOM_SEED_ZERO")) {
        m_randEngine = std::mt19937(0);
    }
#endif

    GC_add_event_callback(gcEventCallback, this);

    g_doubleInSmallValueTag = DoubleInSmallValue(0).getTag();

    g_objectRareDataTag = ObjectRareData(nullptr).getTag();

    g_symbolTag = Symbol(nullptr).getTag();

#define DECLARE_GLOBAL_SYMBOLS(name) m_globalSymbols.name = new Symbol(String::fromASCII("Symbol." #name));
    DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

    ExecutionState stateForInit((Context*)nullptr);

    m_defaultStructureForObject = new ObjectStructureWithTransition(ObjectStructureItemTightVector(), false, false);

    m_defaultStructureForFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.prototype,
                                                                                   ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&functionPrototypeNativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(m_staticStrings.name,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(m_staticStrings.length,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.prototype,
                                                                                          ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(m_staticStrings.name,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(m_staticStrings.length,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForObject->addProperty(m_staticStrings.name,
                                                                                                 ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForNotConstructorFunctionObject->addProperty(m_staticStrings.length,
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


    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForSymbolObject = m_defaultStructureForObject;

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(m_staticStrings.lastIndex,
                                                                                 ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));

    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(ObjectStructurePropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(m_staticStrings.callee, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForObject->addProperty(m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(ObjectStructurePropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(m_staticStrings.caller, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(m_staticStrings.callee, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));

    m_jobQueue = new JobQueue();
}

#if defined(ENABLE_ICU)

#if defined(ENABLE_RUNTIME_ICU_BINDER)
static std::string findTimezone()
{
    return RuntimeICUBinder::ICU::findSystemTimezoneName();
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
        tzset();
    }

    auto u16 = utf8StringToUTF16String(m_timezoneID.data(), m_timezoneID.size());
    m_timezone = vzone_openID(u16.data(), u16.size());
}
#endif

void VMInstance::clearCaches()
{
    m_regexpCache->clear();
    m_cachedUTC = nullptr;
    globalSymbolRegistry().clear();
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

void VMInstance::enqueuePromiseJob(PromiseObject* promise, Job* job)
{
    m_jobQueue->enqueueJob(job);
    m_platform->didPromiseJobEnqueued(job->relatedContext(), promise);
}

bool VMInstance::hasPendingPromiseJob()
{
    return m_jobQueue->hasNextJob();
}

SandBox::SandBoxResult VMInstance::executePendingPromiseJob()
{
    return m_jobQueue->nextJob()->run();
}
}
