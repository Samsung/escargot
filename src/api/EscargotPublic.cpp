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

#include "Escargot.h"
#include "EscargotPublic.h"
#include "parser/ast/Node.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"
#include "parser/CodeBlock.h"
#include "runtime/Global.h"
#include "runtime/ThreadLocal.h"
#include "runtime/Context.h"
#include "runtime/Platform.h"
#include "runtime/FunctionObject.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "runtime/SandBox.h"
#include "runtime/Environment.h"
#include "runtime/SymbolObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/DataViewObject.h"
#include "runtime/DateObject.h"
#include "runtime/StringObject.h"
#include "runtime/NumberObject.h"
#include "runtime/BooleanObject.h"
#include "runtime/RegExpObject.h"
#include "runtime/ModuleNamespaceObject.h"
#include "runtime/JobQueue.h"
#include "runtime/PromiseObject.h"
#include "runtime/ProxyObject.h"
#include "runtime/BackingStore.h"
#include "runtime/ArrayBuffer.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/SetObject.h"
#include "runtime/WeakSetObject.h"
#include "runtime/MapObject.h"
#include "runtime/WeakMapObject.h"
#include "runtime/WeakRefObject.h"
#include "runtime/FinalizationRegistryObject.h"
#include "runtime/GlobalObjectProxyObject.h"
#include "runtime/CompressibleString.h"
#include "runtime/ReloadableString.h"
#include "runtime/Template.h"
#include "runtime/ObjectTemplate.h"
#include "runtime/FunctionTemplate.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/BigInt.h"
#include "runtime/BigIntObject.h"
#include "runtime/SharedArrayBufferObject.h"
#include "runtime/serialization/Serializer.h"
#include "interpreter/ByteCode.h"
#include "api/internal/ValueAdapter.h"
#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCache.h"
#endif
#if defined(ENABLE_WASM)
#include "wasm/WASMOperations.h"
#endif

namespace Escargot {

inline OptionalRef<ValueRef> toOptionalValue(const Value& v)
{
    return OptionalRef<ValueRef>(reinterpret_cast<ValueRef*>(EncodedValue(v).payload()));
}

inline Value toImpl(const OptionalRef<ValueRef>& v)
{
    if (LIKELY(v.hasValue())) {
        return Value(EncodedValue::fromPayload(v.value()));
    }
    return Value(Value::EmptyValue);
}

inline ValueVectorRef* toRef(const EncodedValueVector* v)
{
    return reinterpret_cast<ValueVectorRef*>(const_cast<EncodedValueVector*>(v));
}

inline EncodedValueVector* toImpl(ValueVectorRef* v)
{
    return reinterpret_cast<EncodedValueVector*>(v);
}

inline AtomicStringRef* toRef(const AtomicString& v)
{
    return reinterpret_cast<AtomicStringRef*>(v.string());
}

inline AtomicString toImpl(AtomicStringRef* v)
{
    return AtomicString::fromPayload(reinterpret_cast<void*>(v));
}

class PlatformBridge : public Platform {
public:
    PlatformBridge(PlatformRef* p)
        : m_platform(p)
    {
    }

    virtual ~PlatformBridge()
    {
        // delete PlatformRef, so we don't care about PlatformRef's lifetime
        delete m_platform;
    }

    virtual void* onMallocArrayBufferObjectDataBuffer(size_t sizeInByte) override
    {
        return m_platform->onMallocArrayBufferObjectDataBuffer(sizeInByte);
    }

    virtual void onFreeArrayBufferObjectDataBuffer(void* buffer, size_t sizeInByte, void* deleterData) override
    {
        m_platform->onFreeArrayBufferObjectDataBuffer(buffer, sizeInByte, deleterData);
    }

    virtual void* onReallocArrayBufferObjectDataBuffer(void* oldBuffer, size_t oldSizeInByte, size_t newSizeInByte) override
    {
        return m_platform->onReallocArrayBufferObjectDataBuffer(oldBuffer, oldSizeInByte, newSizeInByte);
    }

    virtual void markJSJobEnqueued(Context* relatedContext) override
    {
        // TODO Job queue should be separately managed for each thread
        m_platform->markJSJobEnqueued(toRef(relatedContext));
    }

    virtual void markJSJobFromAnotherThreadExists(Context* relatedContext) override
    {
        m_platform->markJSJobFromAnotherThreadExists(toRef(relatedContext));
    }

    virtual LoadModuleResult onLoadModule(Context* relatedContext, Script* whereRequestFrom, String* moduleSrc, ModuleType type) override
    {
        LoadModuleResult result;
        auto refResult = m_platform->onLoadModule(toRef(relatedContext), toRef(whereRequestFrom), toRef(moduleSrc), static_cast<Escargot::PlatformRef::ModuleType>(type));

        result.script = toImpl(refResult.script.get());
        result.errorMessage = toImpl(refResult.errorMessage);
        result.errorCode = refResult.errorCode;

        return result;
    }

    virtual void didLoadModule(Context* relatedContext, Optional<Script*> whereRequestFrom, Script* loadedModule) override
    {
        if (whereRequestFrom) {
            m_platform->didLoadModule(toRef(relatedContext), toRef(whereRequestFrom.value()), toRef(loadedModule));
        } else {
            m_platform->didLoadModule(toRef(relatedContext), nullptr, toRef(loadedModule));
        }
    }

    virtual void hostImportModuleDynamically(Context* relatedContext, Script* referrer, String* src, ModuleType type, PromiseObject* promise) override
    {
        m_platform->hostImportModuleDynamically(toRef(relatedContext), toRef(referrer), toRef(src), static_cast<Escargot::PlatformRef::ModuleType>(type), toRef(promise));
    }

    virtual bool canBlockExecution(Context* relatedContext) override
    {
        return m_platform->canBlockExecution(toRef(relatedContext));
    }

    virtual void* allocateThreadLocalCustomData() override
    {
        return m_platform->allocateThreadLocalCustomData();
    }

    virtual void deallocateThreadLocalCustomData() override
    {
        m_platform->deallocateThreadLocalCustomData();
    }

#ifdef ENABLE_CUSTOM_LOGGING
    virtual void customInfoLogger(const char* format, va_list arg) override
    {
        m_platform->customInfoLogger(format, arg);
    }

    virtual void customErrorLogger(const char* format, va_list arg) override
    {
        m_platform->customErrorLogger(format, arg);
    }
#endif

private:
    PlatformRef* m_platform;
};

// making this function with lambda causes "cannot compile this forwarded non-trivially copyable parameter yet" on Windows/ClangCL
static ValueRef* notifyHostImportModuleDynamicallyInnerExecute(ExecutionStateRef* state, PlatformRef::LoadModuleResult loadModuleResult, Script::ModuleData::ModulePromiseObject* promise)
{
    if (loadModuleResult.script) {
        if (loadModuleResult.script.value()->isExecuted()) {
            if (loadModuleResult.script.value()->wasThereErrorOnModuleEvaluation()) {
                state->throwException(loadModuleResult.script.value()->moduleEvaluationError());
            }
        }
    } else {
        state->throwException(ErrorObjectRef::create(state, loadModuleResult.errorCode, loadModuleResult.errorMessage));
    }
    return ValueRef::createUndefined();
}

void PlatformRef::notifyHostImportModuleDynamicallyResult(ContextRef* relatedContext, ScriptRef* referrer, StringRef* src, PromiseObjectRef* promise, LoadModuleResult loadModuleResult)
{
    auto result = Evaluator::execute(relatedContext, notifyHostImportModuleDynamicallyInnerExecute,
                                     loadModuleResult, (Script::ModuleData::ModulePromiseObject*)promise);

    Script::ModuleData::ModulePromiseObject* mp = (Script::ModuleData::ModulePromiseObject*)promise;
    mp->m_referrer = toImpl(referrer);
    if (loadModuleResult.script.hasValue()) {
        mp->m_loadedScript = toImpl(loadModuleResult.script.value());
        if (!mp->m_loadedScript->moduleData()->m_didCallLoadedCallback) {
            Context* ctx = toImpl(relatedContext);
            Global::platform()->didLoadModule(ctx, toImpl(referrer), mp->m_loadedScript);
            mp->m_loadedScript->moduleData()->m_didCallLoadedCallback = true;
        }
    }

    if (result.error) {
        mp->m_value = toImpl(result.error.value());
        Evaluator::execute(relatedContext, [](ExecutionStateRef* state, ValueRef* error, PromiseObjectRef* promise) -> ValueRef* {
            promise->reject(state, promise);
            return ValueRef::createUndefined();
        },
                           result.error.value(), promise);
    } else {
        Evaluator::execute(relatedContext, [](ExecutionStateRef* state, ValueRef* error, PromiseObjectRef* promise) -> ValueRef* {
            promise->fulfill(state, promise);
            return ValueRef::createUndefined();
        },
                           result.error.value(), promise);
    }
}

void* PlatformRef::threadLocalCustomData()
{
    return ThreadLocal::customData();
}

thread_local bool g_globalsInited;
void Globals::initialize(PlatformRef* platform)
{
    // initialize global value or context including thread-local variables
    // this function should be invoked once at the start of the program
    // argument `platform` will be deleted automatically when Globals::finalize called
    RELEASE_ASSERT(!g_globalsInited);
    Global::initialize(new PlatformBridge(platform));
    ThreadLocal::initialize();
    g_globalsInited = true;
}

void Globals::finalize()
{
    // finalize global value or context including thread-local variables
    // this function should be invoked once at the end of the program
    RELEASE_ASSERT(!!g_globalsInited);
    ThreadLocal::finalize();

    // Global::finalize should be called at the end of program
    // because it holds Platform which could be used in other Object's finalizer
    Global::finalize();
    g_globalsInited = false;
}

bool Globals::isInitialized()
{
    return g_globalsInited;
}

void Globals::initializeThread()
{
    // initialize thread-local variables
    // this function should be invoked at the start of sub-thread
    RELEASE_ASSERT(!g_globalsInited);
    ThreadLocal::initialize();
    g_globalsInited = true;
}

void Globals::finalizeThread()
{
    // finalize thread-local variables
    // this function should be invoked once at the end of sub-thread
    RELEASE_ASSERT(!!g_globalsInited);
    ThreadLocal::finalize();
    g_globalsInited = false;
}

bool Globals::supportsThreading()
{
#if defined(ENABLE_THREADING)
    return true;
#else
    return false;
#endif
}

const char* Globals::version()
{
    return ESCARGOT_VERSION;
}

const char* Globals::buildDate()
{
    return ESCARGOT_BUILD_DATE;
}

void* Memory::gcMalloc(size_t siz)
{
    return GC_MALLOC(siz);
}

void* Memory::gcMallocAtomic(size_t siz)
{
    return GC_MALLOC_ATOMIC(siz);
}

void* Memory::gcMallocUncollectable(size_t siz)
{
    return GC_MALLOC_UNCOLLECTABLE(siz);
}

void* Memory::gcMallocAtomicUncollectable(size_t siz)
{
    return GC_MALLOC_ATOMIC_UNCOLLECTABLE(siz);
}

void Memory::gcFree(void* ptr)
{
    GC_FREE(ptr);
}

void Memory::gcRegisterFinalizer(void* ptr, GCAllocatedMemoryFinalizer callback)
{
    if (callback) {
        GC_REGISTER_FINALIZER_NO_ORDER(ptr, [](void* obj, void* data) {
            ((GCAllocatedMemoryFinalizer)data)(obj);
        },
                                       (void*)callback, nullptr, nullptr);
    } else {
        GC_REGISTER_FINALIZER_NO_ORDER(ptr, nullptr, nullptr, nullptr, nullptr);
    }
}

static void ObjectRefFinalizer(PointerValue* self, void* fn)
{
    Memory::GCAllocatedMemoryFinalizer cb = (Memory::GCAllocatedMemoryFinalizer)fn;
    cb(self);
}

void Memory::gcRegisterFinalizer(ObjectRef* ptr, GCAllocatedMemoryFinalizer callback)
{
    toImpl(ptr)->addFinalizer(ObjectRefFinalizer, (void*)callback);
}

void Memory::gcUnregisterFinalizer(ObjectRef* ptr, GCAllocatedMemoryFinalizer callback)
{
    toImpl(ptr)->removeFinalizer(ObjectRefFinalizer, (void*)callback);
}

void Memory::gc()
{
    GC_gcollect_and_unmap();
}

void Memory::setGCFrequency(size_t value)
{
    GC_set_free_space_divisor(value);
}

size_t Memory::heapSize()
{
    return GC_get_heap_size();
}

size_t Memory::totalSize()
{
    return GC_get_total_bytes();
}

void Memory::addGCEventListener(GCEventType type, OnGCEventListener l, void* data)
{
    GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
    GCEventListenerSet::EventListenerVector* listeners = nullptr;

    switch (type) {
    case MARK_START:
        listeners = list.ensureMarkStartListeners();
        break;
    case MARK_END:
        listeners = list.ensureMarkEndListeners();
        break;
    case RECLAIM_START:
        listeners = list.ensureReclaimStartListeners();
        break;
    case RECLAIM_END:
        listeners = list.ensureReclaimEndListeners();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

#ifndef NDEBUG
    auto iter = std::find(listeners->begin(), listeners->end(), std::make_pair(l, data));
    ASSERT(iter == listeners->end());
#endif

    listeners->push_back(std::make_pair(l, data));
}

bool Memory::removeGCEventListener(GCEventType type, OnGCEventListener l, void* data)
{
    GCEventListenerSet& list = ThreadLocal::gcEventListenerSet();
    Optional<GCEventListenerSet::EventListenerVector*> listeners;

    switch (type) {
    case MARK_START:
        listeners = list.markStartListeners();
        break;
    case MARK_END:
        listeners = list.markEndListeners();
        break;
    case RECLAIM_START:
        listeners = list.reclaimStartListeners();
        break;
    case RECLAIM_END:
        listeners = list.reclaimEndListeners();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (listeners) {
        auto iter = std::find(listeners->begin(), listeners->end(), std::make_pair(l, data));
        if (iter != listeners->end()) {
            listeners->erase(iter);
            return true;
        }
    }

    return false;
}

// I store ref count as EncodedValue. this can prevent what bdwgc can see ref count as address (EncodedValue store integer value as odd)
using PersistentValueRefMapImpl = HashMap<ValueRef*, EncodedValue, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<std::pair<ValueRef* const, EncodedValue>>>;

PersistentRefHolder<PersistentValueRefMap> PersistentValueRefMap::create()
{
    return PersistentRefHolder<PersistentValueRefMap>((PersistentValueRefMap*)new (Memory::gcMalloc(sizeof(PersistentValueRefMapImpl))) PersistentValueRefMapImpl());
}

uint32_t PersistentValueRefMap::add(ValueRef* ptr)
{
    auto value = EncodedValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return 0;
    }

    PersistentValueRefMapImpl* self = (PersistentValueRefMapImpl*)this;
    auto iter = self->find(ptr);
    if (iter == self->end()) {
        self->insert(std::make_pair(ptr, EncodedValue(1)));
        return 1;
    } else {
        iter.value() = EncodedValue(iter.value().asUInt32() + 1);
        return iter.value().asUInt32();
    }
}

uint32_t PersistentValueRefMap::remove(ValueRef* ptr)
{
    auto value = EncodedValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return 0;
    }

    PersistentValueRefMapImpl* self = (PersistentValueRefMapImpl*)this;
    auto iter = self->find(ptr);
    if (iter == self->end()) {
        return 0;
    } else {
        if (iter.value().asUInt32() == 1) {
            self->erase(iter);
            return 0;
        } else {
            iter.value() = EncodedValue(iter.value().asUInt32() - 1);
            return iter.value().asUInt32();
        }
    }
}

void PersistentValueRefMap::clear()
{
    PersistentValueRefMapImpl* self = (PersistentValueRefMapImpl*)this;
    self->clear();
}

StringRef* StringRef::createFromASCII(const char* s, size_t len)
{
    return toRef(String::fromASCII(s, len));
}

StringRef* StringRef::createFromUTF8(const char* s, size_t len, bool maybeASCII)
{
    return toRef(String::fromUTF8(s, len, maybeASCII));
}

StringRef* StringRef::createFromUTF16(const char16_t* s, size_t len)
{
    return toRef(new UTF16String(s, len));
}

StringRef* StringRef::createFromLatin1(const unsigned char* s, size_t len)
{
    return toRef(String::fromLatin1(s, len));
}

StringRef* StringRef::createExternalFromASCII(const char* s, size_t len)
{
    return toRef(new ASCIIStringFromExternalMemory(s, len));
}

StringRef* StringRef::createExternalFromLatin1(const unsigned char* s, size_t len)
{
    return toRef(new Latin1StringFromExternalMemory(s, len));
}

StringRef* StringRef::createExternalFromUTF16(const char16_t* s, size_t len)
{
    return toRef(new UTF16StringFromExternalMemory(s, len));
}

bool StringRef::isCompressibleStringEnabled()
{
#if defined(ENABLE_COMPRESSIBLE_STRING)
    return true;
#else
    return false;
#endif
}

#if defined(ENABLE_COMPRESSIBLE_STRING)
StringRef* StringRef::createFromUTF8ToCompressibleString(VMInstanceRef* instance, const char* s, size_t len, bool maybeASCII)
{
    return toRef(String::fromUTF8ToCompressibleString(toImpl(instance), s, len, maybeASCII));
}

StringRef* StringRef::createFromUTF16ToCompressibleString(VMInstanceRef* instance, const char16_t* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(instance), s, len));
}

StringRef* StringRef::createFromASCIIToCompressibleString(VMInstanceRef* instance, const char* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(instance), s, len));
}

StringRef* StringRef::createFromLatin1ToCompressibleString(VMInstanceRef* instance, const unsigned char* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(instance), s, len));
}

void* StringRef::allocateStringDataBufferForCompressibleString(size_t byteLength)
{
    return CompressibleString::allocateStringDataBuffer(byteLength);
}

void StringRef::deallocateStringDataBufferForCompressibleString(void* ptr, size_t byteLength)
{
    CompressibleString::deallocateStringDataBuffer(ptr, byteLength);
}

StringRef* StringRef::createFromAlreadyAllocatedBufferToCompressibleString(VMInstanceRef* instance, void* buffer, size_t stringLen, bool is8Bit)
{
    return toRef(new CompressibleString(toImpl(instance), buffer, stringLen, is8Bit));
}

#else
StringRef* StringRef::createFromUTF8ToCompressibleString(VMInstanceRef* instance, const char* s, size_t len, bool maybeASCII)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromUTF16ToCompressibleString(VMInstanceRef* instance, const char16_t* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromASCIIToCompressibleString(VMInstanceRef* instance, const char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromLatin1ToCompressibleString(VMInstanceRef* instance, const unsigned char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void* StringRef::allocateStringDataBufferForCompressibleString(size_t byteLength)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void StringRef::deallocateStringDataBufferForCompressibleString(void* ptr, size_t byteLength)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
}

StringRef* StringRef::createFromAlreadyAllocatedBufferToCompressibleString(VMInstanceRef* instance, void* buffer, size_t stringLen, bool is8Bit)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable string compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

#endif

bool StringRef::isReloadableStringEnabled()
{
#if defined(ENABLE_RELOADABLE_STRING)
    return true;
#else
    return false;
#endif
}


#if defined(ENABLE_RELOADABLE_STRING)
StringRef* StringRef::createReloadableString(VMInstanceRef* instance,
                                             bool is8BitString, size_t len, void* callbackData,
                                             void* (*loadCallback)(void* callbackData),
                                             void (*unloadCallback)(void* memoryPtr, void* callbackData))
{
    return toRef(new ReloadableString(toImpl(instance), is8BitString, len, callbackData, loadCallback, unloadCallback));
}
#else
StringRef* StringRef::createReloadableString(VMInstanceRef* instance, bool is8BitString, size_t len, void* callbackData,
                                             void* (*loadCallback)(void* callbackData), void (*unloadCallback)(void* memoryPtr, void* callbackData))
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

StringRef* StringRef::emptyString()
{
    return toRef(String::emptyString);
}

char16_t StringRef::charAt(size_t idx)
{
    return toImpl(this)->charAt(idx);
}

size_t StringRef::length()
{
    return toImpl(this)->length();
}

bool StringRef::has8BitContent()
{
    return toImpl(this)->has8BitContent();
}

bool StringRef::hasExternalMemory()
{
    return toImpl(this)->hasExternalMemory();
}

bool StringRef::isCompressibleString()
{
    return toImpl(this)->isCompressibleString();
}

bool StringRef::isReloadableString()
{
    return toImpl(this)->isReloadableString();
}

bool StringRef::isRopeString()
{
    return toImpl(this)->isRopeString();
}

RopeStringRef* StringRef::asRopeString()
{
    return toRef(toImpl(this)->asRopeString());
}

bool StringRef::equals(StringRef* src)
{
    return toImpl(this)->equals(toImpl(src));
}

bool StringRef::equalsWithASCIIString(const char* buf, size_t len)
{
    return toImpl(this)->equals(buf, len);
}

StringRef* StringRef::substring(size_t from, size_t to)
{
    return toRef(toImpl(this)->substring(from, to));
}

std::string StringRef::toStdUTF8String(int options)
{
    return toImpl(this)->toNonGCUTF8StringData(options);
}

StringRef::StringBufferAccessDataRef StringRef::stringBufferAccessData()
{
    StringRef::StringBufferAccessDataRef ref;
    auto implRef = toImpl(this)->bufferAccessData();

    ref.buffer = implRef.buffer;
    ref.has8BitContent = implRef.has8BitContent;
    ref.length = implRef.length;

    return ref;
}

bool RopeStringRef::wasFlattened()
{
    return toImpl(this)->wasFlattened();
}

OptionalRef<StringRef> RopeStringRef::left()
{
    if (toImpl(this)->wasFlattened()) {
        return nullptr;
    } else {
        return toRef(toImpl(this)->left());
    }
}

OptionalRef<StringRef> RopeStringRef::right()
{
    if (toImpl(this)->wasFlattened()) {
        return nullptr;
    } else {
        return toRef(toImpl(this)->right());
    }
}

SymbolRef* SymbolRef::create(OptionalRef<StringRef> desc)
{
    if (desc) {
        return toRef(new Symbol(toImpl(desc.value())));
    } else {
        return toRef(new Symbol());
    }
}

SymbolRef* SymbolRef::fromGlobalSymbolRegistry(VMInstanceRef* vm, StringRef* desc)
{
    return toRef(Symbol::fromGlobalSymbolRegistry(toImpl(vm), toImpl(desc)));
}

StringRef* SymbolRef::descriptionString()
{
    return toRef(toImpl(this)->descriptionString());
}

ValueRef* SymbolRef::descriptionValue()
{
    return toRef(toImpl(this)->descriptionValue());
}

StringRef* SymbolRef::symbolDescriptiveString()
{
    return toRef(toImpl(this)->symbolDescriptiveString());
}

BigIntRef* BigIntRef::create(StringRef* desc, int radix)
{
    return toRef(new BigInt(BigIntData(toImpl(desc), radix)));
}

BigIntRef* BigIntRef::create(int64_t num)
{
    return toRef(new BigInt(num));
}

BigIntRef* BigIntRef::create(uint64_t num)
{
    return toRef(new BigInt(num));
}

StringRef* BigIntRef::toString(int radix)
{
    return toRef(toImpl(this)->toString(radix));
}

double BigIntRef::toNumber()
{
    return toImpl(this)->toNumber();
}

int64_t BigIntRef::toInt64()
{
    return toImpl(this)->toInt64();
}

uint64_t BigIntRef::toUint64()
{
    return toImpl(this)->toUint64();
}

bool BigIntRef::equals(BigIntRef* b)
{
    return toImpl(this)->equals(toImpl(b));
}

bool BigIntRef::equals(StringRef* s)
{
    return toImpl(this)->equals(toImpl(s));
}

bool BigIntRef::equals(double b)
{
    return toImpl(this)->equals(b);
}

bool BigIntRef::lessThan(BigIntRef* b)
{
    return toImpl(this)->lessThan(toImpl(b));
}

bool BigIntRef::lessThanEqual(BigIntRef* b)
{
    return toImpl(this)->lessThanEqual(toImpl(b));
}

bool BigIntRef::greaterThan(BigIntRef* b)
{
    return toImpl(this)->greaterThan(toImpl(b));
}

bool BigIntRef::greaterThanEqual(BigIntRef* b)
{
    return toImpl(this)->greaterThanEqual(toImpl(b));
}

BigIntRef* BigIntRef::addition(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->addition(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::subtraction(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->subtraction(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::multiply(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->multiply(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::division(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->division(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::remainder(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->remainder(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::pow(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->pow(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::bitwiseAnd(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->bitwiseAnd(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::bitwiseOr(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->bitwiseOr(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::bitwiseXor(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->bitwiseXor(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::leftShift(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->leftShift(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::rightShift(ExecutionStateRef* state, BigIntRef* b)
{
    return toRef(toImpl(this)->rightShift(*toImpl(state), toImpl(b)));
}

BigIntRef* BigIntRef::increment(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->increment(*toImpl(state)));
}

BigIntRef* BigIntRef::decrement(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->decrement(*toImpl(state)));
}

BigIntRef* BigIntRef::bitwiseNot(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->bitwiseNot(*toImpl(state)));
}

BigIntRef* BigIntRef::negativeValue(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->negativeValue(*toImpl(state)));
}

bool BigIntRef::isZero()
{
    return toImpl(this)->isZero();
}

bool BigIntRef::isNaN()
{
    return toImpl(this)->isNaN();
}

bool BigIntRef::isInfinity()
{
    return toImpl(this)->isInfinity();
}

bool BigIntRef::isNegative()
{
    return toImpl(this)->isNegative();
}

Evaluator::StackTraceData::StackTraceData()
    : srcName(toRef(String::emptyString))
    , sourceCode(toRef(String::emptyString))
    , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , functionName(toRef(String::emptyString))
    , callee(nullptr)
    , isFunction(false)
    , isConstructor(false)
    , isAssociatedWithJavaScriptCode(false)
    , isEval(false)
{
}

Evaluator::EvaluatorResult::EvaluatorResult()
    : result(ValueRef::createUndefined())
    , error()
{
}

Evaluator::EvaluatorResult::EvaluatorResult(const EvaluatorResult& src)
    : result(src.result)
    , error(src.error)
    , stackTrace(src.stackTrace)
{
}

const Evaluator::EvaluatorResult& Evaluator::EvaluatorResult::operator=(Evaluator::EvaluatorResult& src)
{
    result = src.result;
    error = src.error;
    stackTrace = src.stackTrace;
    return *this;
}

Evaluator::EvaluatorResult::EvaluatorResult(EvaluatorResult&& src)
    : result(src.result)
    , error(src.error)
    , stackTrace(std::move(src.stackTrace))
{
    src.result = ValueRef::createUndefined();
    src.error = nullptr;
}

StringRef* Evaluator::EvaluatorResult::resultOrErrorToString(ContextRef* ctx) const
{
    if (isSuccessful()) {
        return result->toStringWithoutException(ctx);
    } else {
        return ((ValueRef*)error.value())->toStringWithoutException(ctx);
    }
}

static Evaluator::EvaluatorResult toEvaluatorResultRef(SandBox::SandBoxResult& result)
{
    Evaluator::EvaluatorResult r;
    r.error = toOptionalValue(result.error);
    r.result = toRef(result.result);

    if (!result.error.isEmpty()) {
        new (&r.stackTrace) GCManagedVector<Evaluator::StackTraceData>(result.stackTrace.size());
        for (size_t i = 0; i < result.stackTrace.size(); i++) {
            Evaluator::StackTraceData t;
            t.srcName = toRef(result.stackTrace[i].srcName);
            t.sourceCode = toRef(result.stackTrace[i].sourceCode);
            t.loc.index = result.stackTrace[i].loc.index;
            t.loc.line = result.stackTrace[i].loc.line;
            t.loc.column = result.stackTrace[i].loc.column;
            t.functionName = toRef(result.stackTrace[i].functionName);
            if (result.stackTrace[i].callee) {
                t.callee = toRef(result.stackTrace[i].callee.value());
            }
            t.isFunction = result.stackTrace[i].isFunction;
            t.isConstructor = result.stackTrace[i].isConstructor;
            t.isAssociatedWithJavaScriptCode = result.stackTrace[i].isAssociatedWithJavaScriptCode;
            t.isEval = result.stackTrace[i].isEval;
            r.stackTrace[i] = t;
        }
    }

    return r;
}

Evaluator::EvaluatorResult Evaluator::executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData), void* data)
{
    SandBox sb(toImpl(ctx));

    struct DataSender {
        void* fn;
        void* data;
    } sender;

    sender.fn = (void*)runner;
    sender.data = data;

    auto result = sb.run([](ExecutionState& state, void* data) -> Value {
        DataSender* sender = (DataSender*)data;
        ValueRef* (*runner)(ExecutionStateRef * state, void* passedData) = (ValueRef * (*)(ExecutionStateRef * state, void* passedData)) sender->fn;
        return toImpl(runner(toRef(&state), sender->data));
    },
                         &sender);
    return toEvaluatorResultRef(result);
}

Evaluator::EvaluatorResult Evaluator::executeFunction(ContextRef* ctx, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData, void* passedData2), void* data, void* data2)
{
    SandBox sb(toImpl(ctx));

    struct DataSender {
        void* fn;
        void* data;
        void* data2;
    } sender;

    sender.fn = (void*)runner;
    sender.data = data;
    sender.data2 = data2;

    auto result = sb.run([](ExecutionState& state, void* data) -> Value {
        DataSender* sender = (DataSender*)data;
        ValueRef* (*runner)(ExecutionStateRef * state, void* passedData, void* passedData2) = (ValueRef * (*)(ExecutionStateRef * state, void* passedData, void* passedData2)) sender->fn;
        return toImpl(runner(toRef(&state), sender->data, sender->data2));
    },
                         &sender);
    return toEvaluatorResultRef(result);
}

Evaluator::EvaluatorResult Evaluator::executeFunction(ExecutionStateRef* parentState, ValueRef* (*runner)(ExecutionStateRef* state, void* passedData, void* passedData2), void* data, void* data2)
{
    SandBox sb(toImpl(parentState)->context());

    struct DataSender {
        void* fn;
        void* data;
        void* data2;
    } sender;

    sender.fn = (void*)runner;
    sender.data = data;
    sender.data2 = data2;

    auto result = sb.run(*toImpl(parentState), [](ExecutionState& state, void* data) -> Value {
        DataSender* sender = (DataSender*)data;
        ValueRef* (*runner)(ExecutionStateRef * state, void* passedData, void* passedData2) = (ValueRef * (*)(ExecutionStateRef * state, void* passedData, void* passedData2)) sender->fn;
        return toImpl(runner(toRef(&state), sender->data, sender->data2));
    },
                         &sender);
    return toEvaluatorResultRef(result);
}

COMPILE_ASSERT((int)VMInstanceRef::PromiseHookType::Init == (int)VMInstance::PromiseHookType::Init, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseHookType::Resolve == (int)VMInstance::PromiseHookType::Resolve, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseHookType::Before == (int)VMInstance::PromiseHookType::Before, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseHookType::After == (int)VMInstance::PromiseHookType::After, "");

COMPILE_ASSERT((int)VMInstanceRef::PromiseRejectEvent::PromiseRejectWithNoHandler == (int)VMInstance::PromiseRejectEvent::PromiseRejectWithNoHandler, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseRejectEvent::PromiseHandlerAddedAfterReject == (int)VMInstance::PromiseRejectEvent::PromiseHandlerAddedAfterReject, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseRejectEvent::PromiseRejectAfterResolved == (int)VMInstance::PromiseRejectEvent::PromiseRejectAfterResolved, "");
COMPILE_ASSERT((int)VMInstanceRef::PromiseRejectEvent::PromiseResolveAfterResolved == (int)VMInstance::PromiseRejectEvent::PromiseResolveAfterResolved, "");

PersistentRefHolder<VMInstanceRef> VMInstanceRef::create(const char* locale, const char* timezone, const char* baseCacheDir)
{
    return PersistentRefHolder<VMInstanceRef>(toRef(new VMInstance(locale, timezone, baseCacheDir)));
}

void VMInstanceRef::setOnVMInstanceDelete(OnVMInstanceDelete cb)
{
    toImpl(this)->setOnDestroyCallback([](VMInstance* instance, void* data) {
        if (data) {
            (reinterpret_cast<OnVMInstanceDelete>(data))(toRef(instance));
        }
    },
                                       (void*)cb);
}

#if defined(ENABLE_EXTENDED_API)
void VMInstanceRef::registerErrorCreationCallback(ErrorCallback cb)
{
    toImpl(this)->registerErrorCreationCallback([](ExecutionState& state, ErrorObject* err, void* cb) -> void {
        ASSERT(!!cb);
        (reinterpret_cast<ErrorCallback>(cb))(toRef(&state), toRef(err));
    },
                                                (void*)cb);
}

void VMInstanceRef::registerErrorThrowCallback(ErrorCallback cb)
{
    toImpl(this)->registerErrorThrowCallback([](ExecutionState& state, ErrorObject* err, void* cb) -> void {
        ASSERT(!!cb);
        (reinterpret_cast<ErrorCallback>(cb))(toRef(&state), toRef(err));
    },
                                             (void*)cb);
}

void VMInstanceRef::unregisterErrorCreationCallback()
{
    toImpl(this)->unregisterErrorCreationCallback();
}

void VMInstanceRef::unregisterErrorThrowCallback()
{
    toImpl(this)->unregisterErrorThrowCallback();
}
#endif

void VMInstanceRef::registerPromiseHook(PromiseHook promiseHook)
{
    toImpl(this)->registerPromiseHook([](ExecutionState& state, VMInstance::PromiseHookType type, PromiseObject* promise, const Value& parent, void* hook) -> void {
        ASSERT(!!hook);
        (reinterpret_cast<PromiseHook>(hook))(toRef(&state), (PromiseHookType)type, toRef(promise), toRef(parent));
    },
                                      (void*)promiseHook);
}

void VMInstanceRef::unregisterPromiseHook()
{
    toImpl(this)->unregisterPromiseHook();
}

void VMInstanceRef::registerPromiseRejectCallback(PromiseRejectCallback rejectCallback)
{
    toImpl(this)->registerPromiseRejectCallback([](ExecutionState& state, PromiseObject* promise, const Value& value, VMInstance::PromiseRejectEvent event, void* callback) -> void {
        ASSERT(!!callback);
        (reinterpret_cast<PromiseRejectCallback>(callback))(toRef(&state), toRef(promise), toRef(value), (PromiseRejectEvent)(event));
    },
                                                (void*)rejectCallback);
}

void VMInstanceRef::unregisterPromiseRejectCallback()
{
    toImpl(this)->unregisterPromiseRejectCallback();
}

void VMInstanceRef::enterIdleMode()
{
    toImpl(this)->enterIdleMode();
}

void VMInstanceRef::clearCachesRelatedWithContext()
{
    toImpl(this)->clearCachesRelatedWithContext();
}

#define DECLARE_GLOBAL_SYMBOLS(name)                      \
    SymbolRef* VMInstanceRef::name##Symbol()              \
    {                                                     \
        return toRef(toImpl(this)->globalSymbols().name); \
    }
DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

bool VMInstanceRef::hasPendingJob()
{
    return toImpl(this)->hasPendingJob();
}

Evaluator::EvaluatorResult VMInstanceRef::executePendingJob()
{
    auto result = toImpl(this)->executePendingJob();
    return toEvaluatorResultRef(result);
}

bool VMInstanceRef::hasPendingJobFromAnotherThread()
{
    return toImpl(this)->hasPendingJobFromAnotherThread();
}

bool VMInstanceRef::waitEventFromAnotherThread(unsigned timeoutInMillisecond)
{
    return toImpl(this)->waitEventFromAnotherThread(timeoutInMillisecond);
}

void VMInstanceRef::executePendingJobFromAnotherThread()
{
    toImpl(this)->executePendingJobFromAnotherThread();
}

size_t VMInstanceRef::maxCompiledByteCodeSize()
{
    return toImpl(this)->maxCompiledByteCodeSize();
}

void VMInstanceRef::setMaxCompiledByteCodeSize(size_t s)
{
    toImpl(this)->setMaxCompiledByteCodeSize(s);
}

#if defined(ENABLE_CODE_CACHE)
bool VMInstanceRef::isCodeCacheEnabled()
{
    return true;
}

size_t VMInstanceRef::codeCacheMinSourceLength()
{
    return toImpl(this)->codeCache()->minSourceLength();
}

void VMInstanceRef::setCodeCacheMinSourceLength(size_t s)
{
    toImpl(this)->codeCache()->setMinSourceLength(s);
}

size_t VMInstanceRef::codeCacheMaxCacheCount()
{
    return toImpl(this)->codeCache()->maxCacheCount();
}

void VMInstanceRef::setCodeCacheMaxCacheCount(size_t s)
{
    toImpl(this)->codeCache()->setMaxCacheCount(s);
}

bool VMInstanceRef::codeCacheShouldLoadFunctionOnScriptLoading()
{
    return toImpl(this)->codeCache()->shouldLoadFunctionOnScriptLoading();
}

void VMInstanceRef::setCodeCacheShouldLoadFunctionOnScriptLoading(bool s)
{
    toImpl(this)->codeCache()->setShouldLoadFunctionOnScriptLoading(s);
}
#else // ENABLE_CODE_CACHE
bool VMInstanceRef::isCodeCacheEnabled()
{
    return false;
}

size_t VMInstanceRef::codeCacheMinSourceLength()
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}

void VMInstanceRef::setCodeCacheMinSourceLength(size_t s)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}

size_t VMInstanceRef::codeCacheMaxCacheCount()
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}

void VMInstanceRef::setCodeCacheMaxCacheCount(size_t s)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}

bool VMInstanceRef::codeCacheShouldLoadFunctionOnScriptLoading()
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}

void VMInstanceRef::setCodeCacheShouldLoadFunctionOnScriptLoading(bool s)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable code cache");
    RELEASE_ASSERT_NOT_REACHED();
}
#endif // ENABLE_CODE_CACHE

#ifdef ESCARGOT_DEBUGGER

class DebuggerOperationsRef::BreakpointOperations::ObjectStore {
public:
    Vector<Object*, GCUtil::gc_malloc_allocator<Object*>> m_activeObjects;
};

StringRef* DebuggerOperationsRef::BreakpointOperations::eval(StringRef* sourceCode, bool& isError, size_t& objectIndex)
{
    ExecutionState* state = toImpl(m_executionState);
    Debugger* debugger = state->context()->debugger();

    objectIndex = SIZE_MAX;

    if (debugger == nullptr) {
        isError = true;

        return StringRef::createFromASCII("Debugger is not available");
    }

    isError = false;

    String* result;

    debugger->setStopState(ESCARGOT_DEBUGGER_IN_EVAL_MODE);

    try {
        Value asValue(toImpl(sourceCode));
        Value evalResult(Value::ForceUninitialized);
        evalResult = state->context()->globalObject()->evalLocal(*state, asValue, state->thisValue(), reinterpret_cast<ByteCodeBlock*>(weakCodeRef())->m_codeBlock, true);

        if (evalResult.isObject()) {
            result = nullptr;
            objectIndex = putObject(toRef(evalResult.asObject()));
        } else {
            result = evalResult.toStringWithoutException(*state);
        }
    } catch (const Value& val) {
        result = val.toStringWithoutException(*state);

        isError = true;
    }

    debugger->setStopState(ESCARGOT_DEBUGGER_IN_WAIT_MODE);
    return toRef(result);
}

void DebuggerOperationsRef::BreakpointOperations::getStackTrace(DebuggerOperationsRef::DebuggerStackTraceDataVector& outStackTrace)
{
    ExecutionState* state = toImpl(m_executionState);
    StackTraceDataOnStackVector stackTraceDataVector;

    bool hasSavedStackTrace = SandBox::createStackTrace(stackTraceDataVector, *state, true);
    ByteCodeLOCDataMap locMap;
    size_t size = stackTraceDataVector.size();

    outStackTrace.clear();

    for (uint32_t i = 0; i < size; i++) {
        if (reinterpret_cast<size_t>(stackTraceDataVector[i].loc.actualCodeBlock) != SIZE_MAX) {
            ByteCodeBlock* byteCodeBlock = stackTraceDataVector[i].loc.actualCodeBlock;
            size_t line, column;

            if (stackTraceDataVector[i].loc.index == SIZE_MAX) {
                size_t byteCodePosition = stackTraceDataVector[i].loc.byteCodePosition;

                ByteCodeLOCData* locData;
                auto iterMap = locMap.find(byteCodeBlock);
                if (iterMap == locMap.end()) {
                    locData = new ByteCodeLOCData();
                    locMap.insert(std::make_pair(byteCodeBlock, locData));
                } else {
                    locData = iterMap->second;
                }

                ExtendedNodeLOC loc = byteCodeBlock->computeNodeLOCFromByteCode(state->context(), byteCodePosition, byteCodeBlock->m_codeBlock, locData);
                line = (uint32_t)loc.line;
                column = (uint32_t)loc.column;
            } else {
                line = (uint32_t)stackTraceDataVector[i].loc.line;
                column = (uint32_t)stackTraceDataVector[i].loc.column;
            }

            outStackTrace.push_back(DebuggerStackTraceData(reinterpret_cast<WeakCodeRef*>(byteCodeBlock), line, column, stackTraceDataVector[i].executionStateDepth));
        }
    }

    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }

    if (hasSavedStackTrace) {
        Debugger* debugger = state->context()->debugger();

        for (auto iter = debugger->activeSavedStackTrace()->begin(); iter != debugger->activeSavedStackTrace()->end(); iter++) {
            outStackTrace.push_back(DebuggerStackTraceData(reinterpret_cast<WeakCodeRef*>(iter->byteCodeBlock), iter->line, iter->column, SIZE_MAX));
        }
    }
}

void DebuggerOperationsRef::BreakpointOperations::getLexicalScopeChain(uint32_t stateIndex, DebuggerOperationsRef::LexicalScopeChainVector& outLexicalScopeChain)
{
    outLexicalScopeChain.clear();

    ExecutionState* state = toImpl(m_executionState);

    while (stateIndex > 0) {
        state = state->parent();
        stateIndex--;

        if (state == nullptr) {
            return;
        }
    }

    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv) {
        EnvironmentRecord* record = lexEnv->record();
        ScopeType type;

        if (record->isGlobalEnvironmentRecord()) {
            type = GLOBAL_ENVIRONMENT;
        } else if (record->isDeclarativeEnvironmentRecord()) {
            DeclarativeEnvironmentRecord* declarativeRecord = record->asDeclarativeEnvironmentRecord();
            if (declarativeRecord->isFunctionEnvironmentRecord()) {
                type = FUNCTION_ENVIRONMENT;
            } else if (record->isModuleEnvironmentRecord()) {
                type = MODULE_ENVIRONMENT;
            } else {
                type = DECLARATIVE_ENVIRONMENT;
            }
        } else if (record->isObjectEnvironmentRecord()) {
            type = OBJECT_ENVIRONMENT;
        } else {
            type = UNKNOWN_ENVIRONMENT;
        }

        outLexicalScopeChain.push_back(type);
        lexEnv = lexEnv->outerEnvironment();
    }
}

static void fillObjectProperties(ExecutionState* state, Object* object, Object::OwnPropertyKeyVector& keys, DebuggerOperationsRef::PropertyKeyValueVector& result, size_t start)
{
    size_t size = keys.size();

    ASSERT(result.size() >= start + size);

    for (size_t i = 0; i < size; i++) {
        ObjectPropertyName propertyName(*state, keys[i]);

        result[start + i].key = toRef(keys[i].toStringWithoutException(*state));

        try {
            ObjectGetResult value = object->getOwnProperty(*state, propertyName);

            result[start + i].value = toRef(value.value(*state, Value(object)));
        } catch (const Value& val) {
            // The value field is optional, and not filled on error.
        }
    }
}

static void fillRecordProperties(ExecutionState* state, EnvironmentRecord* record, IdentifierRecordVector* identifiers, DebuggerOperationsRef::PropertyKeyValueVector& result)
{
    size_t size = identifiers->size();

    ASSERT(result.size() >= size);

    for (size_t i = 0; i < size; i++) {
        AtomicString name = (*identifiers)[i].m_name;

        result[i].key = toRef(name.string());

        try {
            EnvironmentRecord::GetBindingValueResult value = record->getBindingValue(*state, name);
            ASSERT(value.m_hasBindingValue);

            result[i].value = toRef(value.m_value);
        } catch (const Value& val) {
            // The value field is optional, and not filled on error.
        }
    }
}

static void fillRecordProperties(ExecutionState* state, ModuleEnvironmentRecord* record, const ModuleEnvironmentRecord::ModuleBindingRecordVector& bindings, DebuggerOperationsRef::PropertyKeyValueVector& result)
{
    size_t size = bindings.size();

    ASSERT(result.size() >= size);

    for (size_t i = 0; i < size; i++) {
        AtomicString name = bindings[i].m_localName;

        result[i].key = toRef(name.string());

        try {
            EnvironmentRecord::GetBindingValueResult value = record->getBindingValue(*state, name);
            ASSERT(value.m_hasBindingValue);

            result[i].value = toRef(value.m_value);
        } catch (const Value& val) {
            // The value field is optional, and not filled on error.
        }
    }
}

class DebuggerAPI {
public:
    static IdentifierRecordVector* globalDeclarativeRecord(GlobalEnvironmentRecord* global)
    {
        return global->m_globalDeclarativeRecord;
    }

    static const ModuleEnvironmentRecord::ModuleBindingRecordVector& moduleBindings(ModuleEnvironmentRecord* moduleRecord)
    {
        return moduleRecord->m_moduleBindings;
    }

    static IdentifierRecordVector* declarativeEnvironmentRecordVector(DeclarativeEnvironmentRecord* declarativeRecord)
    {
        return &declarativeRecord->asDeclarativeEnvironmentRecordNotIndexed()->m_recordVector;
    }
};

DebuggerOperationsRef::PropertyKeyValueVector DebuggerOperationsRef::BreakpointOperations::getLexicalScopeChainProperties(uint32_t stateIndex, uint32_t scopeIndex)
{
    ExecutionState* state = toImpl(m_executionState);

    while (stateIndex > 0) {
        state = state->parent();
        stateIndex--;

        if (!state) {
            return PropertyKeyValueVector();
        }
    }

    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv && scopeIndex > 0) {
        lexEnv = lexEnv->outerEnvironment();
        scopeIndex--;
    }

    if (!lexEnv) {
        return PropertyKeyValueVector();
    }

    EnvironmentRecord* record = lexEnv->record();
    if (record->isGlobalEnvironmentRecord()) {
        GlobalEnvironmentRecord* global = record->asGlobalEnvironmentRecord();
        Object* globalObject = global->globalObject();

        Object::OwnPropertyKeyVector keys = globalObject->ownPropertyKeys(*state);
        IdentifierRecordVector* identifiers = DebuggerAPI::globalDeclarativeRecord(global);

        PropertyKeyValueVector result(identifiers->size() + keys.size());

        fillRecordProperties(state, global, identifiers, result);
        fillObjectProperties(state, globalObject, keys, result, identifiers->size());
        return result;
    } else if (record->isDeclarativeEnvironmentRecord()) {
        DeclarativeEnvironmentRecord* declarativeRecord = record->asDeclarativeEnvironmentRecord();
        if (declarativeRecord->isFunctionEnvironmentRecord()) {
            IdentifierRecordVector* identifiers = declarativeRecord->asFunctionEnvironmentRecord()->getRecordVector();

            if (identifiers != NULL) {
                PropertyKeyValueVector result(identifiers->size());

                fillRecordProperties(state, record, identifiers, result);
                return result;
            }
        } else if (record->isModuleEnvironmentRecord()) {
            ModuleEnvironmentRecord* moduleRecord = record->asModuleEnvironmentRecord();

            const ModuleEnvironmentRecord::ModuleBindingRecordVector& bindings = DebuggerAPI::moduleBindings(moduleRecord);
            PropertyKeyValueVector result(bindings.size());

            fillRecordProperties(state, moduleRecord, bindings, result);
            return result;
        } else if (declarativeRecord->isDeclarativeEnvironmentRecordNotIndexed()) {
            IdentifierRecordVector* identifiers = DebuggerAPI::declarativeEnvironmentRecordVector(declarativeRecord);
            PropertyKeyValueVector result(identifiers->size());

            fillRecordProperties(state, record, identifiers, result);
            return result;
        }
    } else if (record->isObjectEnvironmentRecord()) {
        Object* bindingObject = record->asObjectEnvironmentRecord()->bindingObject();
        Object::OwnPropertyKeyVector keys = bindingObject->ownPropertyKeys(*state);
        PropertyKeyValueVector result(keys.size());

        fillObjectProperties(state, bindingObject, keys, result, 0);
        return result;
    }

    return PropertyKeyValueVector();
}

size_t DebuggerOperationsRef::BreakpointOperations::putObject(ObjectRef* object)
{
    Object* storedObject = toImpl(object);

    if (m_objectStore == nullptr) {
        m_objectStore = new ObjectStore();
        m_objectStore->m_activeObjects.pushBack(storedObject);
        return 0;
    }

    size_t size = m_objectStore->m_activeObjects.size();

    for (size_t i = 0; i < size; i++) {
        if (m_objectStore->m_activeObjects[i] == storedObject) {
            return i;
        }
    }

    m_objectStore->m_activeObjects.pushBack(storedObject);
    return size;
}

ObjectRef* DebuggerOperationsRef::BreakpointOperations::getObject(size_t index)
{
    if (m_objectStore == nullptr || index >= m_objectStore->m_activeObjects.size()) {
        return nullptr;
    }

    return toRef(m_objectStore->m_activeObjects[index]);
}

StringRef* DebuggerOperationsRef::getFunctionName(WeakCodeRef* weakCodeRef)
{
    ByteCodeBlock* byteCode = reinterpret_cast<ByteCodeBlock*>(weakCodeRef);

    return toRef(byteCode->codeBlock()->functionName().string());
}

bool DebuggerOperationsRef::updateBreakpoint(WeakCodeRef* weakCodeRef, uint32_t offset, bool enable)
{
    ByteCodeBlock* byteCode = reinterpret_cast<ByteCodeBlock*>(weakCodeRef);

    ByteCode* breakpoint = (ByteCode*)(byteCode->m_code.data() + offset);

#if defined(ESCARGOT_COMPUTED_GOTO_INTERPRETER)
    if (enable) {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointDisabledOpcode]) {
            return false;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointEnabledOpcode];
    } else {
        if (breakpoint->m_opcodeInAddress != g_opcodeTable.m_addressTable[BreakpointEnabledOpcode]) {
            return false;
        }
        breakpoint->m_opcodeInAddress = g_opcodeTable.m_addressTable[BreakpointDisabledOpcode];
    }
#else
    if (enable) {
        if (breakpoint->m_opcode != BreakpointDisabledOpcode) {
            return false;
        }
        breakpoint->m_opcode = BreakpointEnabledOpcode;
    } else {
        if (breakpoint->m_opcode != BreakpointEnabledOpcode) {
            return false;
        }
        breakpoint->m_opcode = BreakpointDisabledOpcode;
    }
#endif

    return true;
}

class DebuggerC : public Debugger {
public:
    virtual void parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error = nullptr) override;
    virtual void stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state) override;
    virtual void byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock) override;
    virtual void exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace) override;
    virtual void consoleOut(String* output) override;
    virtual String* getClientSource(String** sourceName) override;
    virtual bool getWaitBeforeExitClient() override;
    virtual void deleteClient() override;

    DebuggerC(DebuggerOperationsRef::DebuggerClient* debuggerClient, Context* context)
        : m_debuggerClient(debuggerClient)
    {
        enable(context);
    }

protected:
    virtual bool processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest = true) override;

private:
    DebuggerOperationsRef::DebuggerClient* m_debuggerClient;
};

void DebuggerC::parseCompleted(String* source, String* srcName, size_t originLineOffset, String* error)
{
    if (error != nullptr) {
        m_debuggerClient->parseError(toRef(source), toRef(srcName), toRef(error));
        return;
    }

    size_t breakpointLocationsSize = m_breakpointLocationsVector.size();

    if (originLineOffset > 0) {
        for (size_t i = 0; i < breakpointLocationsSize; i++) {
            // adjust line offset for manipulated source code
            // inserted breakpoint's line info should be bigger than `originLineOffset`
            BreakpointLocationVector& locationVector = m_breakpointLocationsVector[i]->breakpointLocations;
            for (size_t j = 0; j < locationVector.size(); j++) {
                ASSERT(locationVector[j].line > originLineOffset);
                locationVector[j].line -= originLineOffset;
            }
        }
    }

    for (size_t i = 0; i < breakpointLocationsSize; i++) {
        InterpretedCodeBlock* codeBlock = reinterpret_cast<InterpretedCodeBlock*>(m_breakpointLocationsVector[i]->weakCodeRef);

        m_breakpointLocationsVector[i]->weakCodeRef = reinterpret_cast<Debugger::WeakCodeRef*>(codeBlock->byteCodeBlock());
    }

    // Same structure, but the definition is duplicated.
    std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>* info = reinterpret_cast<std::vector<DebuggerOperationsRef::BreakpointLocationsInfo*>*>(&m_breakpointLocationsVector);

    m_debuggerClient->parseCompleted(toRef(source), toRef(srcName), *info);
}

static LexicalEnvironment* getFunctionLexEnv(ExecutionState* state)
{
    LexicalEnvironment* lexEnv = state->lexicalEnvironment();

    while (lexEnv) {
        EnvironmentRecord* record = lexEnv->record();

        if (record->isDeclarativeEnvironmentRecord()
            && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            return lexEnv;
        }

        lexEnv = lexEnv->outerEnvironment();
    }
    return nullptr;
}

void DebuggerC::stopAtBreakpoint(ByteCodeBlock* byteCodeBlock, uint32_t offset, ExecutionState* state)
{
    DebuggerOperationsRef::BreakpointOperations operations(reinterpret_cast<DebuggerOperationsRef::WeakCodeRef*>(byteCodeBlock), toRef(state), offset);

    switch (m_debuggerClient->stopAtBreakpoint(operations)) {
    case DebuggerOperationsRef::Continue: {
        m_stopState = nullptr;
        break;
    }
    case DebuggerOperationsRef::Step: {
        m_stopState = ESCARGOT_DEBUGGER_ALWAYS_STOP;
        break;
    }
    case DebuggerOperationsRef::Next: {
        m_stopState = state;
        break;
    }
    case DebuggerOperationsRef::Finish: {
        LexicalEnvironment* lexEnv = getFunctionLexEnv(state);

        if (!lexEnv) {
            m_stopState = nullptr;
            break;
        }

        ExecutionState* stopState = state->parent();

        while (stopState && getFunctionLexEnv(stopState) == lexEnv) {
            stopState = stopState->parent();
        }

        m_stopState = stopState;
        break;
    }
    }
}

void DebuggerC::byteCodeReleaseNotification(ByteCodeBlock* byteCodeBlock)
{
    if (m_debuggerClient) {
        // Debugger could be removed earlier when this function called
        // e.g. release global objects such as Context and VMInstance at the end of execution
        m_debuggerClient->codeReleased(reinterpret_cast<DebuggerOperationsRef::WeakCodeRef*>(byteCodeBlock));
    }
}

void DebuggerC::exceptionCaught(String* message, SavedStackTraceDataVector& exceptionTrace)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(exceptionTrace);
}

void DebuggerC::consoleOut(String* output)
{
    UNUSED_PARAMETER(output);
}

String* DebuggerC::getClientSource(String** sourceName)
{
    UNUSED_PARAMETER(sourceName);

    return nullptr;
}

bool DebuggerC::getWaitBeforeExitClient()
{
    return false;
}

void DebuggerC::deleteClient()
{
    if (m_debuggerClient) {
        // delete DebuggerClient that was created and delivered from the user
        delete m_debuggerClient;
        m_debuggerClient = nullptr;
    }
}

bool DebuggerC::processEvents(ExecutionState* state, Optional<ByteCodeBlock*> byteCodeBlock, bool isBlockingRequest)
{
    UNUSED_PARAMETER(state);
    UNUSED_PARAMETER(byteCodeBlock);
    UNUSED_PARAMETER(isBlockingRequest);

    return false;
}

#endif /* ESCARGOT_DEBUGGER */

PersistentRefHolder<ContextRef> ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = toImpl(vminstanceref);
    return PersistentRefHolder<ContextRef>(toRef(new Context(vminstance)));
}

void ContextRef::clearRelatedQueuedJobs()
{
    Context* imp = toImpl(this);
    imp->vmInstance()->jobQueue()->clearJobRelatedWithSpecificContext(imp);
}

ContextRef* ExecutionStateRef::context()
{
    return toRef(toImpl(this)->context());
}

AtomicStringRef* AtomicStringRef::create(ContextRef* c, const char* src, size_t len)
{
    AtomicString a(toImpl(c), src, len);
    return toRef(a);
}

AtomicStringRef* AtomicStringRef::create(ContextRef* c, StringRef* src)
{
    AtomicString a(toImpl(c), toImpl(src));
    return toRef(a);
}

AtomicStringRef* AtomicStringRef::emptyAtomicString()
{
    AtomicString a;
    return toRef(a);
}

StringRef* AtomicStringRef::string()
{
    return toRef(toImpl(this).string());
}

ScriptParserRef* ContextRef::scriptParser()
{
    Context* imp = toImpl(this);
    return toRef(&imp->scriptParser());
}

ValueVectorRef* ValueVectorRef::create(size_t size)
{
    return toRef(new EncodedValueVector(size));
}

size_t ValueVectorRef::size()
{
    return toImpl(this)->size();
}

void ValueVectorRef::pushBack(ValueRef* val)
{
    toImpl(this)->pushBack(EncodedValueVectorElement::fromPayload(val));
}

void ValueVectorRef::insert(size_t pos, ValueRef* val)
{
    toImpl(this)->insert(pos, EncodedValueVectorElement::fromPayload(val));
}

void ValueVectorRef::erase(size_t pos)
{
    toImpl(this)->erase(pos);
}

void ValueVectorRef::erase(size_t start, size_t end)
{
    toImpl(this)->erase(start, end);
}

ValueRef* ValueVectorRef::at(const size_t idx)
{
    return reinterpret_cast<ValueRef*>((*toImpl(this))[idx].payload());
}

void ValueVectorRef::set(const size_t idx, ValueRef* newValue)
{
    toImpl(this)->data()[idx] = EncodedValueVectorElement::fromPayload(newValue);
}

void ValueVectorRef::resize(size_t newSize)
{
    toImpl(this)->resize(newSize);
}

void* ObjectPropertyDescriptorRef::operator new(size_t size)
{
    return GC_MALLOC(size);
}

void ObjectPropertyDescriptorRef::operator delete(void* ptr)
{
    // destructor of ObjectPropertyDescriptorRef is implicitly invoked
    GC_FREE(ptr);
}

ObjectPropertyDescriptorRef::~ObjectPropertyDescriptorRef()
{
    ASSERT(!!m_privateData);
    ((ObjectPropertyDescriptor*)m_privateData)->~ObjectPropertyDescriptor();
    GC_FREE(m_privateData);
    m_privateData = nullptr;
}

ObjectPropertyDescriptorRef::ObjectPropertyDescriptorRef(void* src)
    : m_privateData(new (GC) ObjectPropertyDescriptor(*((ObjectPropertyDescriptor*)src)))
{
}

ObjectPropertyDescriptorRef::ObjectPropertyDescriptorRef(ValueRef* value)
    : m_privateData(new (GC) ObjectPropertyDescriptor(toImpl(value), ObjectPropertyDescriptor::ValuePresent))
{
}

ObjectPropertyDescriptorRef::ObjectPropertyDescriptorRef(ValueRef* value, bool writable)
    : m_privateData(new (GC) ObjectPropertyDescriptor(toImpl(value),
                                                      (ObjectPropertyDescriptor::PresentAttribute)((writable ? ObjectPropertyDescriptor::WritablePresent : ObjectPropertyDescriptor::NonWritablePresent) | ObjectPropertyDescriptor::ValuePresent)))
{
}

ObjectPropertyDescriptorRef::ObjectPropertyDescriptorRef(ValueRef* getter, ValueRef* setter)
    : m_privateData(new (GC) ObjectPropertyDescriptor(JSGetterSetter(toImpl(getter), toImpl(setter)), ObjectPropertyDescriptor::NotPresent))
{
}

ObjectPropertyDescriptorRef::ObjectPropertyDescriptorRef(const ObjectPropertyDescriptorRef& src)
    : m_privateData(new (GC) ObjectPropertyDescriptor(*((ObjectPropertyDescriptor*)src.m_privateData)))
{
}

const ObjectPropertyDescriptorRef& ObjectPropertyDescriptorRef::operator=(const ObjectPropertyDescriptorRef& src)
{
    ((ObjectPropertyDescriptor*)m_privateData)->~ObjectPropertyDescriptor();
    new (m_privateData) ObjectPropertyDescriptor(*((ObjectPropertyDescriptor*)src.m_privateData));
    return *this;
}

ValueRef* ObjectPropertyDescriptorRef::value() const
{
    return toRef(((ObjectPropertyDescriptor*)m_privateData)->value());
}

bool ObjectPropertyDescriptorRef::hasValue() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isDataDescriptor();
}

ValueRef* ObjectPropertyDescriptorRef::getter() const
{
    return toRef(((ObjectPropertyDescriptor*)m_privateData)->getterSetter().getter());
}

bool ObjectPropertyDescriptorRef::hasGetter() const
{
    return !hasValue() && ((ObjectPropertyDescriptor*)m_privateData)->hasJSGetter();
}

ValueRef* ObjectPropertyDescriptorRef::setter() const
{
    return toRef(((ObjectPropertyDescriptor*)m_privateData)->getterSetter().setter());
}

bool ObjectPropertyDescriptorRef::hasSetter() const
{
    return !hasValue() && ((ObjectPropertyDescriptor*)m_privateData)->hasJSSetter();
}

bool ObjectPropertyDescriptorRef::isEnumerable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isEnumerable();
}

void ObjectPropertyDescriptorRef::setEnumerable(bool enumerable)
{
    ((ObjectPropertyDescriptor*)m_privateData)->setEnumerable(enumerable);
}

bool ObjectPropertyDescriptorRef::hasEnumerable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isEnumerablePresent();
}

void ObjectPropertyDescriptorRef::setConfigurable(bool configurable)
{
    ((ObjectPropertyDescriptor*)m_privateData)->setConfigurable(configurable);
}

bool ObjectPropertyDescriptorRef::isConfigurable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isConfigurable();
}

bool ObjectPropertyDescriptorRef::hasConfigurable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isConfigurablePresent();
}

bool ObjectPropertyDescriptorRef::isWritable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isWritable();
}

bool ObjectPropertyDescriptorRef::hasWritable() const
{
    return ((ObjectPropertyDescriptor*)m_privateData)->isWritablePresent();
}

ObjectRef* ObjectRef::create(ExecutionStateRef* state)
{
#if defined(ESCARGOT_SMALL_CONFIG)
    auto obj = new Object(*toImpl(state));
    obj->markThisObjectDontNeedStructureTransitionTable();
    return toRef(obj);
#else
    return toRef(new Object(*toImpl(state)));
#endif
}

ObjectRef* ObjectRef::create(ExecutionStateRef* state, ObjectRef* proto)
{
#if defined(ESCARGOT_SMALL_CONFIG)
    auto obj = new Object(*toImpl(state), toImpl(proto));
    obj->markThisObjectDontNeedStructureTransitionTable();
    return toRef(obj);
#else
    return toRef(new Object(*toImpl(state), toImpl(proto)));
#endif
}


// can not redefine or delete virtual property
class ExposableObject : public DerivedObject {
public:
    ExposableObject(ExecutionState& state, ExposableObjectGetOwnPropertyCallback getOwnPropetyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback, ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback)
        : DerivedObject(state)
        , m_getOwnPropetyCallback(getOwnPropetyCallback)
        , m_defineOwnPropertyCallback(defineOwnPropertyCallback)
        , m_enumerationCallback(enumerationCallback)
        , m_deleteOwnPropertyCallback(deleteOwnPropertyCallback)
    {
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        Value PV = P.toPlainValue();
        if (!PV.isSymbol()) {
            auto result = m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(PV));
            if (result.m_value.hasValue()) {
                return ObjectGetResult(toImpl(result.m_value.value()), result.m_isWritable, result.m_isEnumerable, result.m_isConfigurable);
            }
        }
        return Object::getOwnProperty(state, P);
    }
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) override
    {
        // Only value type supported
        if (desc.isValuePresent()) {
            Value PV = P.toPlainValue();
            if (!PV.isSymbol() && m_defineOwnPropertyCallback(toRef(&state), toRef(this), toRef(PV), toRef(desc.value()))) {
                return true;
            }
        }
        return DerivedObject::defineOwnProperty(state, P, desc);
    }
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) override
    {
        Value PV = P.toPlainValue();
        if (!PV.isSymbol()) {
            auto result = m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toPlainValue()));
            if (result.m_value.hasValue()) {
                return m_deleteOwnPropertyCallback(toRef(&state), toRef(this), toRef(P.toPlainValue()));
            }
        }
        return Object::deleteOwnProperty(state, P);
    }
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) override
    {
        auto names = m_enumerationCallback(toRef(&state), toRef(this));
        for (size_t i = 0; i < names.size(); i++) {
            int attr = 0;
            if (names[i].m_isWritable) {
                attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::WritablePresent;
            }
            if (names[i].m_isEnumerable) {
                attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::EnumerablePresent;
            }
            if (names[i].m_isConfigurable) {
                attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::ConfigurablePresent;
            }
            ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)attr);

            callback(state, this, ObjectPropertyName(state, toImpl(names[i].m_name)), desc, data);
        }
        Object::enumeration(state, callback, data, shouldSkipSymbolKey);
    }

    virtual bool isInlineCacheable() override
    {
        return false;
    }

private:
    ExposableObjectGetOwnPropertyCallback m_getOwnPropetyCallback;
    ExposableObjectDefineOwnPropertyCallback m_defineOwnPropertyCallback;
    ExposableObjectEnumerationCallback m_enumerationCallback;
    ExposableObjectDeleteOwnPropertyCallback m_deleteOwnPropertyCallback;
};

ObjectRef* ObjectRef::createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback)
{
    return toRef(new ExposableObject(*toImpl(state), getOwnPropertyCallback, defineOwnPropertyCallback, enumerationCallback, deleteOwnPropertyCallback));
}

ValueRef* ObjectRef::get(ExecutionStateRef* state, ValueRef* propertyName)
{
    auto result = toImpl(this)->get(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
    if (result.hasValue()) {
        return toRef(result.value(*toImpl(state), toImpl(this)));
    }
    return ValueRef::createUndefined();
}

ValueRef* ObjectRef::getOwnProperty(ExecutionStateRef* state, ValueRef* propertyName)
{
    auto result = toImpl(this)->getOwnProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
    if (result.hasValue()) {
        return toRef(result.value(*toImpl(state), toImpl(this)));
    }
    return ValueRef::createUndefined();
}

ValueRef* ObjectRef::getOwnPropertyDescriptor(ExecutionStateRef* state, ValueRef* propertyName)
{
    Value name = toImpl(propertyName).toString(*toImpl(state));
    return toRef(toImpl(this)->getOwnPropertyDescriptor(*toImpl(state), ObjectPropertyName(*toImpl(state), name)));
}

void* ObjectRef::NativeDataAccessorPropertyData::operator new(size_t size)
{
    return GC_MALLOC_ATOMIC(size);
}

COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NotPresent == (int)ObjectPropertyDescriptor::NotPresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::WritablePresent == (int)ObjectPropertyDescriptor::WritablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::EnumerablePresent == (int)ObjectPropertyDescriptor::EnumerablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::ConfigurablePresent == (int)ObjectPropertyDescriptor::ConfigurablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonWritablePresent == (int)ObjectPropertyDescriptor::NonWritablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonEnumerablePresent == (int)ObjectPropertyDescriptor::NonEnumerablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonConfigurablePresent == (int)ObjectPropertyDescriptor::NonConfigurablePresent, "");

bool ObjectRef::defineOwnProperty(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* desc)
{
    return toImpl(this)->defineOwnProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)), ObjectPropertyDescriptor(*toImpl(state), toImpl(desc).asObject()));
}

bool ObjectRef::defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, const DataPropertyDescriptor& desc)
{
    return toImpl(this)->defineOwnProperty(*toImpl(state),
                                           ObjectPropertyName(*toImpl(state), toImpl(propertyName)), ObjectPropertyDescriptor(toImpl(desc.m_value), (ObjectPropertyDescriptor::PresentAttribute)desc.m_attribute));
}

bool ObjectRef::defineDataProperty(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    int attr = 0;
    if (isWritable)
        attr = attr | ObjectPropertyDescriptor::WritablePresent;
    else
        attr = attr | ObjectPropertyDescriptor::NonWritablePresent;

    if (isEnumerable)
        attr = attr | ObjectPropertyDescriptor::EnumerablePresent;
    else
        attr = attr | ObjectPropertyDescriptor::NonEnumerablePresent;

    if (isConfigurable)
        attr = attr | ObjectPropertyDescriptor::ConfigurablePresent;
    else
        attr = attr | ObjectPropertyDescriptor::NonConfigurablePresent;
    return toImpl(this)->defineOwnProperty(*toImpl(state),
                                           ObjectPropertyName(*toImpl(state), toImpl(propertyName)), ObjectPropertyDescriptor(toImpl(value), (ObjectPropertyDescriptor::PresentAttribute)attr));
}

bool ObjectRef::defineAccessorProperty(ExecutionStateRef* state, ValueRef* propertyName, const AccessorPropertyDescriptor& desc)
{
    return toImpl(this)->defineOwnProperty(*toImpl(state),
                                           ObjectPropertyName(*toImpl(state), toImpl(propertyName)), ObjectPropertyDescriptor(JSGetterSetter(toImpl(desc.m_getter), toImpl(desc.m_setter)), (ObjectPropertyDescriptor::PresentAttribute)desc.m_attribute));
}

bool ObjectRef::defineNativeDataAccessorProperty(ExecutionStateRef* state, ValueRef* propertyName, NativeDataAccessorPropertyData* publicData, bool actsLikeJSGetterSetter)
{
    ObjectPropertyNativeGetterSetterData* innerData = new ObjectPropertyNativeGetterSetterData(publicData->m_isWritable, publicData->m_isEnumerable, publicData->m_isConfigurable, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
        NativeDataAccessorPropertyData* publicData = reinterpret_cast<NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
        return toImpl(publicData->m_getter(toRef(&state), toRef(self), toRef(receiver), publicData));
    },
                                                                                               nullptr, actsLikeJSGetterSetter);

    if (!publicData->m_isWritable) {
        innerData->m_setter = nullptr;
    } else if (publicData->m_isWritable && !publicData->m_setter) {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            return false;
        };
    } else {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            NativeDataAccessorPropertyData* publicData = reinterpret_cast<NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
            return publicData->m_setter(toRef(&state), toRef(self), toRef(receiver), publicData, toRef(setterInputData));
        };
    }

    return toImpl(this)->defineNativeDataAccessorProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)), innerData, Value(Value::FromPayload, (intptr_t)publicData));
}

bool ObjectRef::set(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value)
{
    return toImpl(this)->set(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)), toImpl(value), toImpl(this));
}

ValueRef* ObjectRef::getIndexedProperty(ExecutionStateRef* state, ValueRef* property)
{
    auto result = toImpl(this)->getIndexedProperty(*toImpl(state), toImpl(property));
    if (result.hasValue()) {
        return toRef(result.value(*toImpl(state), toImpl(this)));
    }
    return ValueRef::createUndefined();
}

bool ObjectRef::setIndexedProperty(ExecutionStateRef* state, ValueRef* property, ValueRef* value)
{
    return toImpl(this)->setIndexedProperty(*toImpl(state), toImpl(property), toImpl(value), toImpl(this));
}

bool ObjectRef::has(ExecutionStateRef* state, ValueRef* propertyName)
{
    return toImpl(this)->hasProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
}

bool ObjectRef::deleteOwnProperty(ExecutionStateRef* state, ValueRef* propertyName)
{
    return toImpl(this)->deleteOwnProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
}

bool ObjectRef::hasOwnProperty(ExecutionStateRef* state, ValueRef* propertyName)
{
    return toImpl(this)->hasOwnProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
}

static bool deletePropertyOperation(ExecutionState& state, Object* object, const ObjectPropertyName& objectPropertyName)
{
    auto hasOwn = object->hasOwnProperty(state, objectPropertyName);
    if (hasOwn) {
        return object->deleteOwnProperty(state, objectPropertyName);
    }
    auto parent = object->getPrototypeObject(state);
    if (parent) {
        return deletePropertyOperation(state, parent, objectPropertyName);
    }
    return false;
}

bool ObjectRef::deleteProperty(ExecutionStateRef* state, ValueRef* propertyName)
{
    ObjectPropertyName objectPropertyName(*toImpl(state), toImpl(propertyName));
    return deletePropertyOperation(*toImpl(state), toImpl(this), objectPropertyName);
}

bool ObjectRef::hasProperty(ExecutionStateRef* state, ValueRef* propertyName)
{
    return toImpl(this)->hasProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)));
}

ValueRef* ObjectRef::getPrototype(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->getPrototype(*toImpl(state)));
}

OptionalRef<ObjectRef> ObjectRef::getPrototypeObject(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);

    auto obj = toImpl(this)->getPrototypeObject(*toImpl(state));
    if (obj != nullptr) {
        return toRef(obj);
    }
    return nullptr;
}

bool ObjectRef::setPrototype(ExecutionStateRef* state, ValueRef* value)
{
    return toImpl(this)->setPrototype(*toImpl(state), toImpl(value));
}

bool ObjectRef::setObjectPrototype(ExecutionStateRef* state, ValueRef* value)
{
    // explicitly call Object::setPrototype
    // could be used for initialization of __proto__to avoid ImmutablePrototypeObject::setPrototype
    // should not be ProxyObject because ProxyObject has it's own setPrototype method
    ASSERT(!toImpl(this)->isProxyObject());
    return toImpl(this)->Object::setPrototype(*toImpl(state), toImpl(value));
}

StringRef* ObjectRef::constructorName(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->constructorName(*toImpl(state)));
}

OptionalRef<ContextRef> ObjectRef::creationContext()
{
    Optional<Object*> o = toImpl(this);

    while (true) {
        if (!o) {
            break;
        }

        if (o->isFunctionObject()) {
            return toRef(o->asFunctionObject()->codeBlock()->context());
        }

        auto ctor = o->readConstructorSlotWithoutState();
        if (ctor) {
            if (ctor.value().isFunction()) {
                return toRef(ctor.value().asFunction()->codeBlock()->context());
            }
        }

        o = o->rawInternalPrototypeObject();
    }

    return OptionalRef<ContextRef>();
}

ValueVectorRef* ObjectRef::ownPropertyKeys(ExecutionStateRef* state)
{
    auto v = toImpl(this)->ownPropertyKeys(*toImpl(state));

    ValueVectorRef* result = ValueVectorRef::create(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        result->set(i, toRef(v[i]));
    }

    return result;
}

uint64_t ObjectRef::length(ExecutionStateRef* state)
{
    return toImpl(this)->length(*toImpl(state));
}

void ObjectRef::setLength(ExecutionStateRef* pState, uint64_t newLength)
{
    auto& state = *toImpl(pState);
    toImpl(this)->set(state, state.context()->staticStrings().length, Value(newLength), toImpl(this));
}

IteratorObjectRef* ObjectRef::values(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->values(*toImpl(state)));
}

IteratorObjectRef* ObjectRef::keys(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->keys(*toImpl(state)));
}

IteratorObjectRef* ObjectRef::entries(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->entries(*toImpl(state)));
}

bool ObjectRef::isExtensible(ExecutionStateRef* state)
{
    return toImpl(this)->isExtensible(*toImpl(state));
}

bool ObjectRef::preventExtensions(ExecutionStateRef* state)
{
    return toImpl(this)->preventExtensions(*toImpl(state));
}

bool ObjectRef::setIntegrityLevel(ExecutionStateRef* state, bool isSealed)
{
    return Object::setIntegrityLevel(*toImpl(state), toImpl(this), isSealed);
}

void* ObjectRef::extraData()
{
    return toImpl(this)->extraData();
}

void ObjectRef::setExtraData(void* e)
{
    toImpl(this)->setExtraData(e);
}

#if defined(ESCARGOT_ENABLE_TEST)
void ObjectRef::setIsHTMLDDA()
{
    toImpl(this)->setIsHTMLDDA();
}
#else
void ObjectRef::setIsHTMLDDA()
{
    // do nothing
}
#endif

void ObjectRef::removeFromHiddenClassChain()
{
    toImpl(this)->markThisObjectDontNeedStructureTransitionTable();
}

// DEPRECATED
void ObjectRef::removeFromHiddenClassChain(ExecutionStateRef* state)
{
    toImpl(this)->markThisObjectDontNeedStructureTransitionTable();
}

void ObjectRef::enumerateObjectOwnProperties(ExecutionStateRef* state, const std::function<bool(ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>& cb, bool shouldSkipSymbolKey)
{
    toImpl(this)->enumeration(*toImpl(state), [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        const std::function<bool(ExecutionStateRef * state, ValueRef * propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>* cb
            = (const std::function<bool(ExecutionStateRef * state, ValueRef * propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>*)data;
        return (*cb)(toRef(&state), toRef(name.toPlainValue()), desc.isWritable(), desc.isEnumerable(), desc.isConfigurable());
    },
                              (void*)&cb, shouldSkipSymbolKey);
}

FunctionObjectRef* GlobalObjectRef::object()
{
    return toRef(toImpl(this)->object());
}

ObjectRef* GlobalObjectRef::objectPrototype()
{
    return toRef(toImpl(this)->objectPrototype());
}

FunctionObjectRef* GlobalObjectRef::objectPrototypeToString()
{
    return toRef(toImpl(this)->objectPrototypeToString());
}

FunctionObjectRef* GlobalObjectRef::function()
{
    return toRef(toImpl(this)->function());
}

FunctionObjectRef* GlobalObjectRef::functionPrototype()
{
    return toRef(toImpl(this)->functionPrototype());
}

FunctionObjectRef* GlobalObjectRef::error()
{
    return toRef(toImpl(this)->error());
}

ObjectRef* GlobalObjectRef::errorPrototype()
{
    return toRef(toImpl(this)->errorPrototype());
}

FunctionObjectRef* GlobalObjectRef::referenceError()
{
    return toRef(toImpl(this)->referenceError());
}

ObjectRef* GlobalObjectRef::referenceErrorPrototype()
{
    return toRef(toImpl(this)->referenceErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::typeError()
{
    return toRef(toImpl(this)->typeError());
}

ObjectRef* GlobalObjectRef::typeErrorPrototype()
{
    return toRef(toImpl(this)->typeErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::rangeError()
{
    return toRef(toImpl(this)->rangeError());
}

ObjectRef* GlobalObjectRef::rangeErrorPrototype()
{
    return toRef(toImpl(this)->rangeErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::syntaxError()
{
    return toRef(toImpl(this)->syntaxError());
}

ObjectRef* GlobalObjectRef::syntaxErrorPrototype()
{
    return toRef(toImpl(this)->syntaxErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::uriError()
{
    return toRef(toImpl(this)->uriError());
}

ObjectRef* GlobalObjectRef::uriErrorPrototype()
{
    return toRef(toImpl(this)->uriErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::evalError()
{
    return toRef(toImpl(this)->evalError());
}

ObjectRef* GlobalObjectRef::evalErrorPrototype()
{
    return toRef(toImpl(this)->evalErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::aggregateError()
{
    return toRef(toImpl(this)->aggregateError());
}

ObjectRef* GlobalObjectRef::aggregateErrorPrototype()
{
    return toRef(toImpl(this)->aggregateErrorPrototype());
}

FunctionObjectRef* GlobalObjectRef::string()
{
    return toRef(toImpl(this)->string());
}

ObjectRef* GlobalObjectRef::stringPrototype()
{
    return toRef(toImpl(this)->stringPrototype());
}

FunctionObjectRef* GlobalObjectRef::number()
{
    return toRef(toImpl(this)->number());
}

ObjectRef* GlobalObjectRef::numberPrototype()
{
    return toRef(toImpl(this)->numberPrototype());
}

FunctionObjectRef* GlobalObjectRef::array()
{
    return toRef(toImpl(this)->array());
}

ObjectRef* GlobalObjectRef::arrayPrototype()
{
    return toRef(toImpl(this)->arrayPrototype());
}

FunctionObjectRef* GlobalObjectRef::boolean()
{
    return toRef(toImpl(this)->boolean());
}

ObjectRef* GlobalObjectRef::booleanPrototype()
{
    return toRef(toImpl(this)->booleanPrototype());
}

FunctionObjectRef* GlobalObjectRef::date()
{
    return toRef(toImpl(this)->date());
}

ObjectRef* GlobalObjectRef::datePrototype()
{
    return toRef(toImpl(this)->datePrototype());
}

ObjectRef* GlobalObjectRef::math()
{
    return toRef(toImpl(this)->math());
}

FunctionObjectRef* GlobalObjectRef::regexp()
{
    return toRef(toImpl(this)->regexp());
}

ObjectRef* GlobalObjectRef::regexpPrototype()
{
    return toRef(toImpl(this)->regexpPrototype());
}

ObjectRef* GlobalObjectRef::json()
{
    return toRef(toImpl(this)->json());
}

FunctionObjectRef* GlobalObjectRef::jsonStringify()
{
    return toRef(toImpl(this)->jsonStringify());
}

FunctionObjectRef* GlobalObjectRef::jsonParse()
{
    return toRef(toImpl(this)->jsonParse());
}

FunctionObjectRef* GlobalObjectRef::promise()
{
    return toRef(toImpl(this)->promise());
}

FunctionObjectRef* GlobalObjectRef::promiseAll()
{
    return toRef(toImpl(this)->promiseAll());
}

FunctionObjectRef* GlobalObjectRef::promiseAllSettled()
{
    return toRef(toImpl(this)->promiseAllSettled());
}

FunctionObjectRef* GlobalObjectRef::promiseAny()
{
    return toRef(toImpl(this)->promiseAny());
}

FunctionObjectRef* GlobalObjectRef::promiseRace()
{
    return toRef(toImpl(this)->promiseRace());
}

FunctionObjectRef* GlobalObjectRef::promiseReject()
{
    return toRef(toImpl(this)->promiseReject());
}

FunctionObjectRef* GlobalObjectRef::promiseResolve()
{
    return toRef(toImpl(this)->promiseResolve());
}

ObjectRef* GlobalObjectRef::promisePrototype()
{
    return toRef(toImpl(this)->promisePrototype());
}

FunctionObjectRef* GlobalObjectRef::arrayBuffer()
{
    return toRef(toImpl(this)->arrayBuffer());
}

ObjectRef* GlobalObjectRef::arrayBufferPrototype()
{
    return toRef(toImpl(this)->arrayBufferPrototype());
}

FunctionObjectRef* GlobalObjectRef::dataView()
{
    return toRef(toImpl(this)->dataView());
}

ObjectRef* GlobalObjectRef::dataViewPrototype()
{
    return toRef(toImpl(this)->dataViewPrototype());
}

ObjectRef* GlobalObjectRef::int8Array()
{
    return toRef(toImpl(this)->int8Array());
}

ObjectRef* GlobalObjectRef::int8ArrayPrototype()
{
    return toRef(toImpl(this)->int8ArrayPrototype());
}

ObjectRef* GlobalObjectRef::uint8Array()
{
    return toRef(toImpl(this)->uint8Array());
}

ObjectRef* GlobalObjectRef::uint8ArrayPrototype()
{
    return toRef(toImpl(this)->uint8ArrayPrototype());
}

ObjectRef* GlobalObjectRef::int16Array()
{
    return toRef(toImpl(this)->int16Array());
}

ObjectRef* GlobalObjectRef::int16ArrayPrototype()
{
    return toRef(toImpl(this)->int16ArrayPrototype());
}

ObjectRef* GlobalObjectRef::uint16Array()
{
    return toRef(toImpl(this)->uint16Array());
}

ObjectRef* GlobalObjectRef::uint16ArrayPrototype()
{
    return toRef(toImpl(this)->uint16ArrayPrototype());
}

ObjectRef* GlobalObjectRef::int32Array()
{
    return toRef(toImpl(this)->int32Array());
}

ObjectRef* GlobalObjectRef::int32ArrayPrototype()
{
    return toRef(toImpl(this)->int32ArrayPrototype());
}

ObjectRef* GlobalObjectRef::uint32Array()
{
    return toRef(toImpl(this)->uint32Array());
}

ObjectRef* GlobalObjectRef::uint32ArrayPrototype()
{
    return toRef(toImpl(this)->uint32ArrayPrototype());
}

ObjectRef* GlobalObjectRef::uint8ClampedArray()
{
    return toRef(toImpl(this)->uint8ClampedArray());
}

ObjectRef* GlobalObjectRef::uint8ClampedArrayPrototype()
{
    return toRef(toImpl(this)->uint8ClampedArrayPrototype());
}

ObjectRef* GlobalObjectRef::float32Array()
{
    return toRef(toImpl(this)->float32Array());
}

ObjectRef* GlobalObjectRef::float32ArrayPrototype()
{
    return toRef(toImpl(this)->float32ArrayPrototype());
}

ObjectRef* GlobalObjectRef::float64Array()
{
    return toRef(toImpl(this)->float64Array());
}

ObjectRef* GlobalObjectRef::float64ArrayPrototype()
{
    return toRef(toImpl(this)->float64ArrayPrototype());
}

ObjectRef* GlobalObjectRef::bigInt64Array()
{
    return toRef(toImpl(this)->bigInt64Array());
}

ObjectRef* GlobalObjectRef::bigInt64ArrayPrototype()
{
    return toRef(toImpl(this)->bigInt64ArrayPrototype());
}

ObjectRef* GlobalObjectRef::bigUint64Array()
{
    return toRef(toImpl(this)->bigUint64Array());
}

ObjectRef* GlobalObjectRef::bigUint64ArrayPrototype()
{
    return toRef(toImpl(this)->bigUint64ArrayPrototype());
}

class CallPublicFunctionData : public gc {
public:
    CallPublicFunctionData(FunctionObjectRef::NativeFunctionPointer publicFn)
        : m_publicFn(publicFn)
    {
    }

    FunctionObjectRef::NativeFunctionPointer m_publicFn;
};

class CallPublicFunctionWithNewTargetData : public gc {
public:
    CallPublicFunctionWithNewTargetData(FunctionObjectRef::NativeFunctionWithNewTargetPointer publicFn)
        : m_publicFn(publicFn)
    {
    }

    FunctionObjectRef::NativeFunctionWithNewTargetPointer m_publicFn;
};

static Value publicFunctionBridge(ExecutionState& state, Value thisValue, size_t calledArgc, Value* calledArgv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* func = state.resolveCallee()->asExtendedNativeFunctionObject();
    CallPublicFunctionData* code = func->internalSlotAsPointer<CallPublicFunctionData>(FunctionObjectRef::BuiltinFunctionSlot::PublicFunctionIndex);

    ValueRef** newArgv = ALLOCA(sizeof(ValueRef*) * calledArgc, ValueRef*);
    for (size_t i = 0; i < calledArgc; i++) {
        newArgv[i] = toRef(calledArgv[i]);
    }

    return toImpl(code->m_publicFn(toRef(&state), toRef(thisValue), calledArgc, newArgv, newTarget.hasValue()));
}

static Value publicFunctionBridgeWithNewTarget(ExecutionState& state, Value thisValue, size_t calledArgc, Value* calledArgv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* func = state.resolveCallee()->asExtendedNativeFunctionObject();
    CallPublicFunctionWithNewTargetData* code = func->internalSlotAsPointer<CallPublicFunctionWithNewTargetData>(FunctionObjectRef::BuiltinFunctionSlot::PublicFunctionIndex);

    ValueRef** newArgv = ALLOCA(sizeof(ValueRef*) * calledArgc, ValueRef*);
    for (size_t i = 0; i < calledArgc; i++) {
        newArgv[i] = toRef(calledArgv[i]);
    }

    OptionalRef<ObjectRef> newTargetRef;
    if (newTarget) {
        newTargetRef = toRef(newTarget.value());
    }

    return toImpl(code->m_publicFn(toRef(&state), toRef(thisValue), calledArgc, newArgv, newTargetRef));
}

typedef void (*SecurityCheckCallback)(ExecutionStateRef* state, GlobalObjectProxyObjectRef* proxy, GlobalObjectRef* targetGlobalObject);

GlobalObjectProxyObjectRef* GlobalObjectProxyObjectRef::create(ExecutionStateRef* state, GlobalObjectRef* target, SecurityCheckCallback callback)
{
    return toRef(new GlobalObjectProxyObject(*toImpl(state), toImpl(target), callback));
}

GlobalObjectRef* GlobalObjectProxyObjectRef::target()
{
    return toRef(toImpl(this)->target());
}

void GlobalObjectProxyObjectRef::setTarget(GlobalObjectRef* target)
{
    toImpl(this)->setTarget(toImpl(target));
}

static FunctionObjectRef* createFunction(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info, bool isBuiltin)
{
    int flags = 0;
    flags |= info.m_isStrict ? NativeFunctionInfo::Strict : 0;
    flags |= info.m_isConstructor ? NativeFunctionInfo::Constructor : 0;
    NativeFunctionInfo nativeInfo(toImpl(info.m_name), nullptr, info.m_argumentCount, flags);

    if (info.m_hasWithNewTargetCallback) {
        nativeInfo.m_nativeFunction = publicFunctionBridgeWithNewTarget;
    } else {
        nativeInfo.m_nativeFunction = publicFunctionBridge;
    }

    ExtendedNativeFunctionObject* func;
    if (isBuiltin) {
        func = new ExtendedNativeFunctionObjectImpl<1>(*toImpl(state), nativeInfo, NativeFunctionObject::__ForBuiltinConstructor__);
    } else {
        func = new ExtendedNativeFunctionObjectImpl<1>(*toImpl(state), nativeInfo);
    }

    if (info.m_hasWithNewTargetCallback) {
        CallPublicFunctionWithNewTargetData* data = new CallPublicFunctionWithNewTargetData(info.m_nativeFunctionWithNewTarget);
        func->setInternalSlotAsPointer(FunctionObjectRef::BuiltinFunctionSlot::PublicFunctionIndex, data);
    } else {
        CallPublicFunctionData* data = new CallPublicFunctionData(info.m_nativeFunction);
        func->setInternalSlotAsPointer(FunctionObjectRef::BuiltinFunctionSlot::PublicFunctionIndex, data);
    }

    return toRef(func);
}

FunctionObjectRef* FunctionObjectRef::create(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info)
{
    return createFunction(state, info, false);
}

FunctionObjectRef* FunctionObjectRef::createBuiltinFunction(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info)
{
    return createFunction(state, info, true);
}

FunctionObjectRef* FunctionObjectRef::create(ExecutionStateRef* stateRef, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* body)
{
    ExecutionState& state = *toImpl(stateRef);
    Value* newArgv = ALLOCA(sizeof(Value) * argumentCount, Value);
    for (size_t i = 0; i < argumentCount; i++) {
        newArgv[i] = toImpl(argumentNameArray[i]);
    }

    auto functionSource = FunctionObject::createDynamicFunctionScript(state, toImpl(functionName), argumentCount, newArgv, toImpl(body), false, false, false, false);

    Object* proto = state.context()->globalObject()->functionPrototype();
    ScriptFunctionObject* result = new ScriptFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment, true, false);

    return toRef(result);
}

FunctionObjectRef* FunctionObjectRef::create(ExecutionStateRef* stateRef, StringRef* sourceName, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* body)
{
    ASSERT(toImpl(sourceName));

    ExecutionState& state = *toImpl(stateRef);
    Value* newArgv = ALLOCA(sizeof(Value) * argumentCount, Value);
    for (size_t i = 0; i < argumentCount; i++) {
        newArgv[i] = toImpl(argumentNameArray[i]);
    }

    auto functionSource = FunctionObject::createDynamicFunctionScript(state, toImpl(functionName), argumentCount, newArgv, toImpl(body), false, false, false, false, false, toImpl(sourceName));

    Object* proto = state.context()->globalObject()->functionPrototype();
    ScriptFunctionObject* result = new ScriptFunctionObject(state, proto, functionSource.codeBlock, functionSource.outerEnvironment, true, false);

    return toRef(result);
}

ContextRef* FunctionObjectRef::context()
{
    return toRef(toImpl(this)->codeBlock()->context());
}

ValueRef* FunctionObjectRef::getFunctionPrototype(ExecutionStateRef* state)
{
    FunctionObject* o = toImpl(this);
    return toRef(o->getFunctionPrototype(*toImpl(state)));
}

bool FunctionObjectRef::setFunctionPrototype(ExecutionStateRef* state, ValueRef* v)
{
    FunctionObject* o = toImpl(this);
    return o->setFunctionPrototype(*toImpl(state), toImpl(v));
}

bool FunctionObjectRef::isConstructor()
{
    FunctionObject* o = toImpl(this);
    return o->isConstructor();
}

static void markEvalToCodeblock(InterpretedCodeBlock* cb)
{
    cb->setHasEval();
    cb->computeVariables();

    if (cb->hasChildren()) {
        InterpretedCodeBlockVector& childrenVector = cb->children();
        for (size_t i = 0; i < childrenVector.size(); i++) {
            markEvalToCodeblock(childrenVector[i]);
        }
    }
}

void FunctionObjectRef::markFunctionNeedsSlowVirtualIdentifierOperation()
{
    FunctionObject* o = toImpl(this);
    if (o->isScriptFunctionObject()) {
        ScriptFunctionObject* func = o->asScriptFunctionObject();
        markEvalToCodeblock(func->interpretedCodeBlock());
        func->interpretedCodeBlock()->setNeedsVirtualIDOperation();
    }
}

bool FunctionObjectRef::setName(AtomicStringRef* name)
{
    return toImpl(this)->setName(toImpl(name));
}

GlobalObjectRef* ContextRef::globalObject()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->globalObject());
}

ObjectRef* ContextRef::globalObjectProxy()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->globalObjectProxy());
}

void ContextRef::setGlobalObjectProxy(ObjectRef* newGlobalObjectProxy)
{
    Context* ctx = toImpl(this);
    ctx->setGlobalObjectProxy(toImpl(newGlobalObjectProxy));
}

VMInstanceRef* ContextRef::vmInstance()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->vmInstance());
}

bool ContextRef::canThrowException()
{
    return toImpl(this)->canThrowException();
}

void ContextRef::throwException(ValueRef* exceptionValue)
{
    ExecutionState s(toImpl(this));
    toImpl(this)->throwException(s, toImpl(exceptionValue));
}

bool ContextRef::initDebugger(DebuggerOperationsRef::DebuggerClient* debuggerClient)
{
#ifdef ESCARGOT_DEBUGGER
    Context* context = toImpl(this);

    if (debuggerClient == nullptr || context->debuggerEnabled()) {
        return false;
    }

    new DebuggerC(debuggerClient, context);
    return true;
#else /* !ESCARGOT_DEBUGGER */
    return false;
#endif /* ESCARGOT_DEBUGGER */
}

bool ContextRef::disableDebugger()
{
#ifdef ESCARGOT_DEBUGGER
    Context* context = toImpl(this);
    if (context->debugger()) {
        context->debugger()->deleteClient();
    }
    return true;
#else
    return false;
#endif
}

bool ContextRef::initDebuggerRemote(const char* options)
{
#ifdef ESCARGOT_DEBUGGER
    return toImpl(this)->initDebuggerRemote(options);
#else /* !ESCARGOT_DEBUGGER */
    return false;
#endif /* ESCARGOT_DEBUGGER */
}

bool ContextRef::isDebuggerRunning()
{
#ifdef ESCARGOT_DEBUGGER
    return toImpl(this)->debuggerEnabled();
#else /* !ESCARGOT_DEBUGGER */
    return false;
#endif /* ESCARGOT_DEBUGGER */
}

bool ContextRef::isWaitBeforeExit()
{
#ifdef ESCARGOT_DEBUGGER
    return isDebuggerRunning() && toImpl(this)->debugger()->getWaitBeforeExitClient();
#else /* !ESCARGOT_DEBUGGER */
    return false;
#endif /* ESCARGOT_DEBUGGER */
}

void ContextRef::printDebugger(StringRef* output)
{
#ifdef ESCARGOT_DEBUGGER
    toImpl(this)->printDebugger(toImpl(output));
#endif /* ESCARGOT_DEBUGGER */
}

void ContextRef::pumpDebuggerEvents()
{
#ifdef ESCARGOT_DEBUGGER
    toImpl(this)->pumpDebuggerEvents();
#endif /* ESCARGOT_DEBUGGER */
}

void ContextRef::setAsAlwaysStopState()
{
#ifdef ESCARGOT_DEBUGGER
    toImpl(this)->setAsAlwaysStopState();
#endif /* ESCARGOT_DEBUGGER */
}

StringRef* ContextRef::getClientSource(StringRef** sourceName)
{
#ifdef ESCARGOT_DEBUGGER
    return toRef(toImpl(this)->getClientSource(reinterpret_cast<String**>(sourceName)));
#else /* ESCARGOT_DEBUGGER */
    return nullptr;
#endif /* ESCARGOT_DEBUGGER */
}

void ContextRef::setVirtualIdentifierCallback(VirtualIdentifierCallback cb)
{
    toImpl(this)->setVirtualIdentifierCallback([](ExecutionState& state, Value name) -> Value {
        if (state.context()->m_virtualIdentifierCallbackPublic && !name.isSymbol()) {
            return toImpl(((VirtualIdentifierCallback)state.context()->m_virtualIdentifierCallbackPublic)(toRef(&state), toRef(name)));
        }
        return Value(Value::EmptyValue);
    },
                                               (void*)cb);
}

ContextRef::VirtualIdentifierCallback ContextRef::virtualIdentifierCallback()
{
    Context* ctx = toImpl(this);
    return ((VirtualIdentifierCallback)ctx->m_virtualIdentifierCallbackPublic);
}

void ContextRef::setSecurityPolicyCheckCallback(SecurityPolicyCheckCallback cb)
{
    toImpl(this)->setSecurityPolicyCheckCallback([](ExecutionState& state, bool isEval) -> Value {
        if (state.context()->m_securityPolicyCheckCallbackPublic) {
            return toImpl(((SecurityPolicyCheckCallback)state.context()->m_securityPolicyCheckCallbackPublic)(toRef(&state), isEval));
        }
        return Value(Value::EmptyValue);
    },
                                                 (void*)cb);
}

StackOverflowDisabler::StackOverflowDisabler(ExecutionStateRef* es)
    : m_executionState(es)
    , m_originStackLimit(ThreadLocal::stackLimit())
{
#ifdef STACK_GROWS_DOWN
    size_t newStackLimit = 0;
#else
    size_t newStackLimit = SIZE_MAX;
#endif
    ExecutionState* state = toImpl(es);

    // We assume that StackOverflowDisabler should not be nested-called
    ASSERT(m_originStackLimit != newStackLimit);
    ASSERT(m_originStackLimit == ThreadLocal::stackLimit());
    ThreadLocal::g_stackLimit = newStackLimit;
}

StackOverflowDisabler::~StackOverflowDisabler()
{
    ASSERT(!!m_executionState);
    ThreadLocal::g_stackLimit = m_originStackLimit;
}

OptionalRef<FunctionObjectRef> ExecutionStateRef::resolveCallee()
{
    auto ec = toImpl(this);
    if (ec != nullptr) {
        auto callee = ec->resolveCallee();
        if (callee != nullptr) {
            return toRef(callee);
        }
    }
    return nullptr;
}

GCManagedVector<FunctionObjectRef*> ExecutionStateRef::resolveCallstack()
{
    ExecutionState* state = toImpl(this);

    ExecutionState* pstate = state;
    size_t count = 0;
    while (pstate) {
        if (pstate->lexicalEnvironment() && pstate->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord()
            && pstate->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            count++;
        }
        pstate = pstate->parent();
    }

    GCManagedVector<FunctionObjectRef*> result(count);

    size_t idx = 0;

    pstate = state;
    while (pstate) {
        if (pstate->lexicalEnvironment() && pstate->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord()
            && pstate->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            result[idx++] = toRef(pstate->resolveCallee());
        }
        pstate = pstate->parent();
    }

    return result;
}

GlobalObjectRef* ExecutionStateRef::resolveCallerLexicalGlobalObject()
{
    ASSERT(toImpl(this)->parent());
    auto ctx = toImpl(this)->context();
    auto p = toImpl(this)->parent();
    while (p) {
        if (ctx != p->context()) {
            ctx = p->context();
            break;
        }
        p = p->parent();
    }
    return toRef(ctx->globalObject());
}

#if defined(ENABLE_EXTENDED_API)
bool ExecutionStateRef::onTry()
{
    return toImpl(this)->onTry();
}

bool ExecutionStateRef::onCatch()
{
    return toImpl(this)->onCatch();
}

bool ExecutionStateRef::onFinally()
{
    return toImpl(this)->onFinally();
}
#endif

void ExecutionStateRef::throwException(ValueRef* value)
{
    ExecutionState* imp = toImpl(this);
    imp->throwException(toImpl(value));
}

void ExecutionStateRef::checkStackOverflow()
{
    ExecutionState& imp = *toImpl(this);
    CHECK_STACK_OVERFLOW(imp);
}

GCManagedVector<Evaluator::StackTraceData> ExecutionStateRef::computeStackTrace()
{
    ExecutionState* state = toImpl(this);

    StackTraceDataOnStackVector stackTraceDataVector;
    SandBox::createStackTrace(stackTraceDataVector, *state);

    GCManagedVector<Evaluator::StackTraceData> result(stackTraceDataVector.size());
    ByteCodeLOCDataMap locMap;
    for (size_t i = 0; i < stackTraceDataVector.size(); i++) {
        if (stackTraceDataVector[i].loc.index == SIZE_MAX && reinterpret_cast<size_t>(stackTraceDataVector[i].loc.actualCodeBlock) != SIZE_MAX) {
            ByteCodeBlock* byteCodeBlock = stackTraceDataVector[i].loc.actualCodeBlock;

            ByteCodeLOCData* locData;
            auto iterMap = locMap.find(byteCodeBlock);
            if (iterMap == locMap.end()) {
                locData = new ByteCodeLOCData();
                locMap.insert(std::make_pair(byteCodeBlock, locData));
            } else {
                locData = iterMap->second;
            }

            InterpretedCodeBlock* codeBlock = byteCodeBlock->codeBlock();
            size_t byteCodePosition = stackTraceDataVector[i].loc.byteCodePosition;
            stackTraceDataVector[i].loc = byteCodeBlock->computeNodeLOCFromByteCode(state->context(), byteCodePosition, byteCodeBlock->m_codeBlock, locData);

            stackTraceDataVector[i].srcName = codeBlock->script()->srcName();
            stackTraceDataVector[i].sourceCode = codeBlock->script()->sourceCode();
        }

        Evaluator::StackTraceData t;
        t.srcName = toRef(stackTraceDataVector[i].srcName);
        t.sourceCode = toRef(stackTraceDataVector[i].sourceCode);
        t.loc.index = stackTraceDataVector[i].loc.index;
        t.loc.line = stackTraceDataVector[i].loc.line;
        t.loc.column = stackTraceDataVector[i].loc.column;
        t.functionName = toRef(stackTraceDataVector[i].functionName);
        if (stackTraceDataVector[i].callee) {
            t.callee = toRef(stackTraceDataVector[i].callee.value());
        }
        t.isFunction = stackTraceDataVector[i].isFunction;
        t.isConstructor = stackTraceDataVector[i].isConstructor;
        t.isAssociatedWithJavaScriptCode = stackTraceDataVector[i].isAssociatedWithJavaScriptCode;
        t.isEval = stackTraceDataVector[i].isEval;
        result[i] = t;
    }
    for (auto iter = locMap.begin(); iter != locMap.end(); iter++) {
        delete iter->second;
    }

    return result;
}

OptionalRef<ExecutionStateRef> ExecutionStateRef::parent()
{
    ExecutionState* imp = toImpl(this);
    return toRef(imp->parent());
}

// ValueRef
#define DEFINE_VALUEREF_POINTERVALUE_IS_AS(Name)                                                                     \
    bool ValueRef::is##Name() { return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->is##Name(); } \
    Name##Ref* ValueRef::as##Name() { return toRef(toImpl(this).asPointerValue()->as##Name()); }

ESCARGOT_POINTERVALUE_CHILD_REF_LIST(DEFINE_VALUEREF_POINTERVALUE_IS_AS);
#undef DEFINE_VALUEREF_POINTERVALUE_IS_AS


#define DEFINE_VALUEREF_TYPEDARRAY_IS_AS(Name, name, siz, nativeType)                                         \
    bool ValueRef::is##Name##ArrayObject()                                                                    \
    {                                                                                                         \
        return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isTypedArrayObject()           \
            && toImpl(this).asPointerValue()->asTypedArrayObject()->typedArrayType() == TypedArrayType::Name; \
    }                                                                                                         \
                                                                                                              \
    Name##ArrayObjectRef* ValueRef::as##Name##ArrayObject()                                                   \
    {                                                                                                         \
        return toRef((Name##ArrayObject*)(toImpl(this).asPointerValue()->asTypedArrayObject()));              \
    }

FOR_EACH_TYPEDARRAY_TYPES(DEFINE_VALUEREF_TYPEDARRAY_IS_AS);
#undef DEFINE_VALUEREF_TYPEDARRAY_IS_AS

bool ValueRef::isBoolean()
{
    return toImpl(this).isBoolean();
}

bool ValueRef::isStoredInHeap()
{
    auto value = EncodedValue::fromPayload(this);
    if (value.isStoredInHeap()) {
        return true;
    }
    return false;
}

bool ValueRef::isNumber()
{
    return toImpl(this).isNumber();
}

bool ValueRef::isNull()
{
    return toImpl(this).isNull();
}

bool ValueRef::isUndefined()
{
    return toImpl(this).isUndefined();
}

bool ValueRef::isPointerValue()
{
    return toImpl(this).isPointerValue();
}

bool ValueRef::isInt32()
{
    return toImpl(this).isInt32();
}

bool ValueRef::isUInt32()
{
    return toImpl(this).isUInt32();
}

bool ValueRef::isDouble()
{
    return toImpl(this).isDouble();
}

bool ValueRef::isTrue()
{
    return toImpl(this).isTrue();
}

bool ValueRef::isFalse()
{
    return toImpl(this).isFalse();
}

bool ValueRef::isCallable()
{
    return toImpl(this).isCallable();
}

bool ValueRef::isConstructible() // can ValueRef::construct
{
    return toImpl(this).isConstructor();
}

bool ValueRef::isArgumentsObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isArgumentsObject();
}

bool ValueRef::isArrayPrototypeObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isArrayPrototypeObject();
}

bool ValueRef::isGeneratorFunctionObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isFunctionObject() && toImpl(this).asPointerValue()->asFunctionObject()->isGenerator();
}

bool ValueRef::isGeneratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isGeneratorObject();
}

bool ValueRef::isMapIteratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isMapIteratorObject();
}

bool ValueRef::isSetIteratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isSetIteratorObject();
}

bool ValueRef::isTypedArrayObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isTypedArrayObject();
}

bool ValueRef::isTypedArrayPrototypeObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isTypedArrayPrototypeObject();
}

bool ValueRef::isModuleNamespaceObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isModuleNamespaceObject();
}

bool ValueRef::asBoolean()
{
    return toImpl(this).asBoolean();
}

double ValueRef::asNumber()
{
    return toImpl(this).asNumber();
}

int32_t ValueRef::asInt32()
{
    return toImpl(this).asInt32();
}

uint32_t ValueRef::asUInt32()
{
    return toImpl(this).asUInt32();
}

PointerValueRef* ValueRef::asPointerValue()
{
    return toRef(toImpl(this).asPointerValue());
}

ValueRef* ValueRef::call(ExecutionStateRef* state, ValueRef* receiver, const size_t argc, ValueRef** argv)
{
    auto impl = toImpl(this);
    if (UNLIKELY(!impl.isPointerValue())) {
        ErrorObject::throwBuiltinError(*toImpl(state), ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    PointerValue* o = impl.asPointerValue();
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(Object::call(*toImpl(state), o, toImpl(receiver), argc, newArgv));
}

ValueRef* ValueRef::construct(ExecutionStateRef* state, const size_t argc, ValueRef** argv)
{
    auto impl = toImpl(this);
    if (UNLIKELY(!impl.isPointerValue())) {
        ErrorObject::throwBuiltinError(*toImpl(state), ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    PointerValue* o = impl.asPointerValue();
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(Object::construct(*toImpl(state), o, argc, newArgv));
}

void ValueRef::callConstructor(ExecutionStateRef* state, ObjectRef* receiver, const size_t argc, ValueRef** argv)
{
    auto impl = toImpl(this);
    if (UNLIKELY(!impl.isObject())) {
        ErrorObject::throwBuiltinError(*toImpl(state), ErrorCode::TypeError, ErrorObject::Messages::NOT_Callable);
    }
    Object* o = impl.asObject();
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }

    Object::callConstructor(*toImpl(state), o, toImpl(receiver), argc, newArgv);
}

ValueRef* ValueRef::create(bool value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(int value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(float value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(Value::DoubleToIntConvertibleTestNeeds, value)).payload());
}

ValueRef* ValueRef::create(double value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(Value::DoubleToIntConvertibleTestNeeds, value)).payload());
}

ValueRef* ValueRef::create(long value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned long value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(long long value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned long long value)
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(value)).payload());
}

ValueRef* ValueRef::createNull()
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(Value::Null))
                                           .payload());
}

ValueRef* ValueRef::createUndefined()
{
    return reinterpret_cast<ValueRef*>(EncodedValue(Value(Value::Undefined))
                                           .payload());
}

bool ValueRef::toBoolean(ExecutionStateRef* es)
{
    UNUSED_PARAMETER(es);
    return toImpl(this).toBoolean();
}

double ValueRef::toNumber(ExecutionStateRef* es)
{
    return toImpl(this).toNumber(*toImpl(es));
}

double ValueRef::toInteger(ExecutionStateRef* es)
{
    return toImpl(this).toInteger(*toImpl(es));
}

double ValueRef::toLength(ExecutionStateRef* es)
{
    return toImpl(this).toLength(*toImpl(es));
}

int32_t ValueRef::toInt32(ExecutionStateRef* es)
{
    return toImpl(this).toInt32(*toImpl(es));
}

uint32_t ValueRef::toUint32(ExecutionStateRef* es)
{
    return toImpl(this).toUint32(*toImpl(es));
}

StringRef* ValueRef::toString(ExecutionStateRef* es)
{
    return toRef(toImpl(this).toString(*toImpl(es)));
}

StringRef* ValueRef::toStringWithoutException(ContextRef* ctx)
{
    // declare temporal ExecutionState and SandBox
    ExecutionState state(toImpl(ctx));
    SandBox sb(toImpl(ctx));
    return toRef(toImpl(this).toStringWithoutException(state));
}

ObjectRef* ValueRef::toObject(ExecutionStateRef* es)
{
    return toRef(toImpl(this).toObject(*toImpl(es)));
}

ValueRef::ValueIndex ValueRef::toIndex(ExecutionStateRef* state)
{
    return toImpl(this).toIndex(*toImpl(state));
}

ValueRef::ValueIndex ValueRef::tryToUseAsIndex(ExecutionStateRef* state)
{
    return toImpl(this).tryToUseAsIndex(*toImpl(state));
}

uint32_t ValueRef::toIndex32(ExecutionStateRef* state)
{
    return toImpl(this).toIndex32(*toImpl(state));
}

uint32_t ValueRef::tryToUseAsIndex32(ExecutionStateRef* state)
{
    return toImpl(this).tryToUseAsIndex32(*toImpl(state));
}

uint32_t ValueRef::tryToUseAsIndexProperty(ExecutionStateRef* state)
{
    return toImpl(this).tryToUseAsIndexProperty(*toImpl(state));
}

bool ValueRef::abstractEqualsTo(ExecutionStateRef* state, const ValueRef* other) const
{
    return toImpl(this).abstractEqualsTo(*toImpl(state), toImpl(other));
}

bool ValueRef::equalsTo(ExecutionStateRef* state, const ValueRef* other) const
{
    return toImpl(this).equalsTo(*toImpl(state), toImpl(other));
}

bool ValueRef::instanceOf(ExecutionStateRef* state, const ValueRef* other) const
{
    return toImpl(this).instanceOf(*toImpl(state), toImpl(other));
}

ValueRef* IteratorObjectRef::next(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->next(*toImpl(state)));
}

class GenericIteratorObject : public IteratorObject {
public:
    GenericIteratorObject(ExecutionState& state, GenericIteratorObjectRef::GenericIteratorObjectRefCallback callback, void* callbackData)
        : IteratorObject(state, state.context()->globalObject()->genericIteratorPrototype())
        , m_callback(callback)
        , m_callbackData(callbackData)
    {
    }

    virtual bool isGenericIteratorObject() const override
    {
        return true;
    }

    virtual std::pair<Value, bool> advance(ExecutionState& state) override
    {
        auto ret = m_callback(toRef(&state), m_callbackData);
        return std::make_pair(toImpl(ret.first), ret.second);
    }

private:
    GenericIteratorObjectRef::GenericIteratorObjectRefCallback m_callback;
    void* m_callbackData;
};

GenericIteratorObjectRef* GenericIteratorObjectRef::create(ExecutionStateRef* state, GenericIteratorObjectRefCallback callback, void* callbackData)
{
    return toRef(new GenericIteratorObject(*toImpl(state), callback, callbackData));
}

ArrayObjectRef* ArrayObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new ArrayObject(*toImpl(state)));
}

ArrayObjectRef* ArrayObjectRef::create(ExecutionStateRef* state, const uint64_t size)
{
    return toRef(new ArrayObject(*toImpl(state), size));
}

ArrayObjectRef* ArrayObjectRef::create(ExecutionStateRef* state, ValueVectorRef* source)
{
    uint64_t sourceSize = source->size();
    ArrayObject* ret = new ArrayObject(*toImpl(state), sourceSize);

    for (size_t i = 0; i < sourceSize; i++) {
        ret->setIndexedProperty(*toImpl(state), Value(i), toImpl(source->at(i)), ret);
    }

    return toRef(ret);
}

COMPILE_ASSERT((int)ErrorCode::None == (int)ErrorObjectRef::Code::None, "");
COMPILE_ASSERT((int)ErrorCode::ReferenceError == (int)ErrorObjectRef::Code::ReferenceError, "");
COMPILE_ASSERT((int)ErrorCode::TypeError == (int)ErrorObjectRef::Code::TypeError, "");
COMPILE_ASSERT((int)ErrorCode::SyntaxError == (int)ErrorObjectRef::Code::SyntaxError, "");
COMPILE_ASSERT((int)ErrorCode::RangeError == (int)ErrorObjectRef::Code::RangeError, "");
COMPILE_ASSERT((int)ErrorCode::URIError == (int)ErrorObjectRef::Code::URIError, "");
COMPILE_ASSERT((int)ErrorCode::EvalError == (int)ErrorObjectRef::Code::EvalError, "");

ErrorObjectRef* ErrorObjectRef::create(ExecutionStateRef* state, ErrorObjectRef::Code code, StringRef* errorMessage)
{
    return toRef(ErrorObject::createError(*toImpl(state), (ErrorCode)code, toImpl(errorMessage)));
}

void ErrorObjectRef::updateStackTraceData(ExecutionStateRef* state)
{
    toImpl(this)->updateStackTraceData(*toImpl(state));
}

ReferenceErrorObjectRef* ReferenceErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((ReferenceErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::ReferenceError, toImpl(errorMessage)));
}

TypeErrorObjectRef* TypeErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((TypeErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::TypeError, toImpl(errorMessage)));
}

SyntaxErrorObjectRef* SyntaxErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((SyntaxErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::SyntaxError, toImpl(errorMessage)));
}

RangeErrorObjectRef* RangeErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((RangeErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::RangeError, toImpl(errorMessage)));
}

URIErrorObjectRef* URIErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((URIErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::URIError, toImpl(errorMessage)));
}

EvalErrorObjectRef* EvalErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((EvalErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::EvalError, toImpl(errorMessage)));
}

AggregateErrorObjectRef* AggregateErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef((AggregateErrorObject*)ErrorObject::createError(*toImpl(state), ErrorCode::AggregateError, toImpl(errorMessage)));
}

DateObjectRef* DateObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new DateObject(*toImpl(state)));
}

int64_t DateObjectRef::currentTime()
{
    return DateObject::currentTime();
}

void DateObjectRef::setTimeValue(ExecutionStateRef* state, ValueRef* str)
{
    toImpl(this)->setTimeValue(*toImpl(state), toImpl(str));
}

void DateObjectRef::setTimeValue(int64_t value)
{
    toImpl(this)->setTimeValue(value);
}

StringRef* DateObjectRef::toUTCString(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->toUTCString(*toImpl(state), toImpl(state->context())->staticStrings().toUTCString.string()));
}

double DateObjectRef::primitiveValue()
{
    return toImpl(this)->primitiveValue();
}

StringObjectRef* StringObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new StringObject(*toImpl(state)));
}

void StringObjectRef::setPrimitiveValue(ExecutionStateRef* state, ValueRef* str)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(str).toString(*toImpl(state)));
}

StringRef* StringObjectRef::primitiveValue()
{
    return toRef(toImpl(this)->primitiveValue());
}

SymbolObjectRef* SymbolObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new SymbolObject(*toImpl(state), new Symbol()));
}

void SymbolObjectRef::setPrimitiveValue(ExecutionStateRef* state, SymbolRef* value)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(value));
}

SymbolRef* SymbolObjectRef::primitiveValue()
{
    return toRef(toImpl(this)->primitiveValue());
}

void BigIntObjectRef::setPrimitiveValue(ExecutionStateRef* state, BigIntRef* value)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(value));
}

BigIntRef* BigIntObjectRef::primitiveValue()
{
    return toRef(toImpl(this)->primitiveValue());
}

NumberObjectRef* NumberObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new NumberObject(*toImpl(state)));
}

void NumberObjectRef::setPrimitiveValue(ExecutionStateRef* state, ValueRef* str)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(str).toNumber(*toImpl(state)));
}

double NumberObjectRef::primitiveValue()
{
    return toImpl(this)->primitiveValue();
}

BooleanObjectRef* BooleanObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new BooleanObject(*toImpl(state)));
}

void BooleanObjectRef::setPrimitiveValue(ExecutionStateRef* state, ValueRef* str)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(str).toBoolean());
}

bool BooleanObjectRef::primitiveValue()
{
    return toImpl(this)->primitiveValue();
}

RegExpObjectRef* RegExpObjectRef::create(ExecutionStateRef* state, ValueRef* source, ValueRef* option)
{
    return toRef(new RegExpObject(*toImpl(state), toImpl(source).toString(*toImpl(state)), toImpl(option).toString(*toImpl(state))));
}

RegExpObjectRef* RegExpObjectRef::create(ExecutionStateRef* state, ValueRef* source, RegExpObjectOption option)
{
    return toRef(new RegExpObject(*toImpl(state), toImpl(source).toString(*toImpl(state)), option));
}

bool RegExpObjectRef::match(ExecutionStateRef* state, ValueRef* str, RegExpObjectRef::RegexMatchResult& result, bool testOnly, size_t startIndex)
{
    return toImpl(this)->match(*toImpl(state), toImpl(str).toString(*toImpl(state)), (Escargot::RegexMatchResult&)result, testOnly, startIndex);
}

StringRef* RegExpObjectRef::source()
{
    return toRef(toImpl(this)->source());
}

RegExpObjectRef::RegExpObjectOption RegExpObjectRef::option()
{
    return (RegExpObjectRef::RegExpObjectOption)toImpl(this)->option();
}

BackingStoreRef* BackingStoreRef::createDefaultNonSharedBackingStore(size_t byteLength)
{
    return toRef(BackingStore::createDefaultNonSharedBackingStore(byteLength));
}

BackingStoreRef* BackingStoreRef::createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreRef::BackingStoreRefDeleterCallback callback, void* callbackData)
{
    return toRef(BackingStore::createNonSharedBackingStore(data, byteLength, (BackingStoreDeleterCallback)callback, callbackData));
}

#if defined(ENABLE_THREADING)
BackingStoreRef* BackingStoreRef::createDefaultSharedBackingStore(size_t byteLength)
{
    return toRef(BackingStore::createDefaultSharedBackingStore(byteLength));
}

BackingStoreRef* BackingStoreRef::createSharedBackingStore(BackingStoreRef* backingStore)
{
    return toRef(BackingStore::createSharedBackingStore(toImpl(backingStore)->sharedDataBlockInfo()));
}
#else
BackingStoreRef* BackingStoreRef::createDefaultSharedBackingStore(size_t byteLength)
{
    RELEASE_ASSERT_NOT_REACHED();
}
BackingStoreRef* BackingStoreRef::createSharedBackingStore(BackingStoreRef* backingStore)
{
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

void* BackingStoreRef::data()
{
    return toImpl(this)->data();
}

size_t BackingStoreRef::byteLength()
{
    return toImpl(this)->byteLength();
}

bool BackingStoreRef::isShared()
{
    return toImpl(this)->isShared();
}

void BackingStoreRef::reallocate(size_t newByteLength)
{
    toImpl(this)->reallocate(newByteLength);
}

OptionalRef<BackingStoreRef> ArrayBufferRef::backingStore()
{
    if (toImpl(this)->backingStore()) {
        return toRef(toImpl(this)->backingStore().value());
    } else {
        return nullptr;
    }
}

uint8_t* ArrayBufferRef::rawBuffer()
{
    return (uint8_t*)toImpl(this)->data();
}

size_t ArrayBufferRef::byteLength()
{
    return toImpl(this)->byteLength();
}

ArrayBufferObjectRef* ArrayBufferObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new ArrayBufferObject(*toImpl(state)));
}

void ArrayBufferObjectRef::allocateBuffer(ExecutionStateRef* state, size_t bytelength)
{
    toImpl(this)->allocateBuffer(*toImpl(state), bytelength);
}

void ArrayBufferObjectRef::attachBuffer(BackingStoreRef* backingStore)
{
    toImpl(this)->attachBuffer(toImpl(backingStore));
}

void ArrayBufferObjectRef::detachArrayBuffer()
{
    toImpl(this)->detachArrayBuffer();
}

bool ArrayBufferObjectRef::isDetachedBuffer()
{
    return toImpl(this)->isDetachedBuffer();
}
#if defined(ENABLE_THREADING)
SharedArrayBufferObjectRef* SharedArrayBufferObjectRef::create(ExecutionStateRef* state, size_t byteLength)
{
    return toRef(new SharedArrayBufferObject(*toImpl(state), toImpl(state)->context()->globalObject()->sharedArrayBufferPrototype(), byteLength));
}

SharedArrayBufferObjectRef* SharedArrayBufferObjectRef::create(ExecutionStateRef* state, BackingStoreRef* backingStore)
{
    return toRef(new SharedArrayBufferObject(*toImpl(state), toImpl(state)->context()->globalObject()->sharedArrayBufferPrototype(), toImpl(backingStore)));
}
#else
SharedArrayBufferObjectRef* SharedArrayBufferObjectRef::create(ExecutionStateRef* state, size_t byteLength)
{
    RELEASE_ASSERT_NOT_REACHED();
}

SharedArrayBufferObjectRef* SharedArrayBufferObjectRef::create(ExecutionStateRef* state, BackingStoreRef* backingStore)
{
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

ArrayBufferRef* ArrayBufferViewRef::buffer()
{
    return toRef(toImpl(this)->buffer());
}

void ArrayBufferViewRef::setBuffer(ArrayBufferRef* bo, size_t byteOffset, size_t byteLength, size_t arrayLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength, arrayLength);
}

void ArrayBufferViewRef::setBuffer(ArrayBufferRef* bo, size_t byteOffset, size_t byteLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength, 0);
}

uint8_t* ArrayBufferViewRef::rawBuffer()
{
    return toImpl(this)->rawBuffer();
}

size_t ArrayBufferViewRef::byteLength()
{
    return toImpl(this)->byteLength();
}

size_t ArrayBufferViewRef::byteOffset()
{
    return toImpl(this)->byteOffset();
}

size_t ArrayBufferViewRef::arrayLength()
{
    return toImpl(this)->arrayLength();
}

DataViewObjectRef* DataViewObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new DataViewObject(*toImpl(state)));
}

Int8ArrayObjectRef* Int8ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Int8ArrayObject(*toImpl(state)));
}

Uint8ArrayObjectRef* Uint8ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Uint8ArrayObject(*toImpl(state)));
}

Int16ArrayObjectRef* Int16ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Int16ArrayObject(*toImpl(state)));
}

Uint16ArrayObjectRef* Uint16ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Uint16ArrayObject(*toImpl(state)));
}

Uint32ArrayObjectRef* Uint32ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Uint32ArrayObject(*toImpl(state)));
}

Int32ArrayObjectRef* Int32ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Int32ArrayObject(*toImpl(state)));
}

Float32ArrayObjectRef* Float32ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Float32ArrayObject(*toImpl(state)));
}

Float64ArrayObjectRef* Float64ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Float64ArrayObject(*toImpl(state)));
}

BigInt64ArrayObjectRef* BigInt64ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new BigInt64ArrayObject(*toImpl(state)));
}

BigUint64ArrayObjectRef* BigUint64ArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new BigUint64ArrayObject(*toImpl(state)));
}

Uint8ClampedArrayObjectRef* Uint8ClampedArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Uint8ClampedArrayObject(*toImpl(state)));
}

PromiseObjectRef* PromiseObjectRef::create(ExecutionStateRef* state)
{
    auto promise = new PromiseObject(*toImpl(state));
    if (UNLIKELY(toImpl(state)->context()->vmInstance()->isPromiseHookRegistered())) {
        toImpl(state)->context()->vmInstance()->triggerPromiseHook(*toImpl(state), VMInstance::PromiseHookType::Init, promise);
    }
    return toRef(promise);
}

COMPILE_ASSERT((int)PromiseObjectRef::PromiseState::Pending == (int)PromiseObject::PromiseState::Pending, "");
COMPILE_ASSERT((int)PromiseObjectRef::PromiseState::FulFilled == (int)PromiseObject::PromiseState::FulFilled, "");
COMPILE_ASSERT((int)PromiseObjectRef::PromiseState::FulFilled == (int)PromiseObject::PromiseState::FulFilled, "");

PromiseObjectRef::PromiseState PromiseObjectRef::state()
{
    return (PromiseObjectRef::PromiseState)toImpl(this)->state();
}

ValueRef* PromiseObjectRef::promiseResult()
{
    return toRef(toImpl(this)->promiseResult());
}

ObjectRef* PromiseObjectRef::then(ExecutionStateRef* state, ValueRef* handler)
{
    return toRef(toImpl(this)->then(*toImpl(state), toImpl(handler)));
}

ObjectRef* PromiseObjectRef::catchOperation(ExecutionStateRef* state, ValueRef* handler)
{
    return toRef(toImpl(this)->catchOperation(*toImpl(state), toImpl(handler)));
}

ObjectRef* PromiseObjectRef::then(ExecutionStateRef* state, ValueRef* onFulfilled, ValueRef* onRejected)
{
    return toRef(toImpl(this)->then(*toImpl(state), toImpl(onFulfilled), toImpl(onRejected), toImpl(this)->newPromiseResultCapability(*toImpl(state))).value());
}

void PromiseObjectRef::fulfill(ExecutionStateRef* state, ValueRef* value)
{
    if (UNLIKELY(toImpl(state)->context()->vmInstance()->isPromiseHookRegistered())) {
        toImpl(state)->context()->vmInstance()->triggerPromiseHook(*toImpl(state), VMInstance::PromiseHookType::Resolve, toImpl(this));
    }
    toImpl(this)->fulfill(*toImpl(state), toImpl(value));
}

void PromiseObjectRef::reject(ExecutionStateRef* state, ValueRef* reason)
{
    if (UNLIKELY(toImpl(state)->context()->vmInstance()->isPromiseHookRegistered())) {
        toImpl(state)->context()->vmInstance()->triggerPromiseHook(*toImpl(state), VMInstance::PromiseHookType::Resolve, toImpl(this));
    }
    toImpl(this)->reject(*toImpl(state), toImpl(reason));
}

bool PromiseObjectRef::hasResolveHandlers()
{
    return toImpl(this)->hasResolveHandlers();
}

bool PromiseObjectRef::hasRejectHandlers()
{
    return toImpl(this)->hasRejectHandlers();
}

ProxyObjectRef* ProxyObjectRef::create(ExecutionStateRef* state, ObjectRef* target, ObjectRef* handler)
{
    return toRef(ProxyObject::createProxy(*toImpl(state), toImpl(target), toImpl(handler)));
}

ObjectRef* ProxyObjectRef::target()
{
    return toRef(toImpl(this)->target());
}

ObjectRef* ProxyObjectRef::handler()
{
    return toRef(toImpl(this)->handler());
}

bool ProxyObjectRef::isRevoked()
{
    return (toImpl(this)->target() == nullptr) && (toImpl(this)->handler() == nullptr);
}

void ProxyObjectRef::revoke()
{
    toImpl(this)->setTarget(nullptr);
    toImpl(this)->setHandler(nullptr);
}

SetObjectRef* SetObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new SetObject(*toImpl(state)));
}

void SetObjectRef::add(ExecutionStateRef* state, ValueRef* key)
{
    toImpl(this)->add(*toImpl(state), toImpl(key));
}

void SetObjectRef::clear(ExecutionStateRef* state)
{
    toImpl(this)->clear(*toImpl(state));
}

bool SetObjectRef::deleteOperation(ExecutionStateRef* state, ValueRef* key)
{
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key));
}

bool SetObjectRef::has(ExecutionStateRef* state, ValueRef* key)
{
    return toImpl(this)->has(*toImpl(state), toImpl(key));
}

size_t SetObjectRef::size(ExecutionStateRef* state)
{
    return toImpl(this)->size(*toImpl(state));
}

WeakSetObjectRef* WeakSetObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new WeakSetObject(*toImpl(state)));
}

bool WeakSetObjectRef::add(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return false;
    }
    toImpl(this)->add(*toImpl(state), toImpl(key).asPointerValue());
    return true;
}

bool WeakSetObjectRef::deleteOperation(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return false;
    }
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key).asPointerValue());
}

bool WeakSetObjectRef::has(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return false;
    }
    return toImpl(this)->has(*toImpl(state), toImpl(key).asPointerValue());
}

MapObjectRef* MapObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new MapObject(*toImpl(state)));
}

void MapObjectRef::clear(ExecutionStateRef* state)
{
    toImpl(this)->clear(*toImpl(state));
}

ValueRef* MapObjectRef::get(ExecutionStateRef* state, ValueRef* key)
{
    return toRef(toImpl(this)->get(*toImpl(state), toImpl(key)));
}

void MapObjectRef::set(ExecutionStateRef* state, ValueRef* key, ValueRef* value)
{
    toImpl(this)->set(*toImpl(state), toImpl(key), toImpl(value));
}

bool MapObjectRef::deleteOperation(ExecutionStateRef* state, ValueRef* key)
{
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key));
}

bool MapObjectRef::has(ExecutionStateRef* state, ValueRef* key)
{
    return toImpl(this)->has(*toImpl(state), toImpl(key));
}

size_t MapObjectRef::size(ExecutionStateRef* state)
{
    return toImpl(this)->size(*toImpl(state));
}

WeakMapObjectRef* WeakMapObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new WeakMapObject(*toImpl(state)));
}

bool WeakMapObjectRef::deleteOperation(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return false;
    }
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key).asPointerValue());
}

ValueRef* WeakMapObjectRef::get(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return ValueRef::createUndefined();
    }
    return toRef(toImpl(this)->get(*toImpl(state), toImpl(key).asPointerValue()));
}

void WeakMapObjectRef::set(ExecutionStateRef* state, ValueRef* key, ValueRef* value)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return;
    }
    toImpl(this)->set(*toImpl(state), toImpl(key).asPointerValue(), toImpl(value));
}

bool WeakMapObjectRef::has(ExecutionStateRef* state, ValueRef* key)
{
    if (!toImpl(key).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return false;
    }
    return toImpl(this)->has(*toImpl(state), toImpl(key).asPointerValue());
}

OptionalRef<WeakRefObjectRef> WeakRefObjectRef::create(ExecutionStateRef* state, ValueRef* target)
{
    if (!toImpl(target).canBeHeldWeakly(toImpl(state)->context()->vmInstance())) {
        return nullptr;
    }
    return OptionalRef<WeakRefObjectRef>(toRef(new WeakRefObject(*toImpl(state), toImpl(target).asPointerValue())));
}

bool WeakRefObjectRef::deleteOperation(ExecutionStateRef* state)
{
    return toImpl(this)->deleteOperation(*toImpl(state));
}

OptionalRef<PointerValueRef> WeakRefObjectRef::deref()
{
    if (toImpl(this)->target()) {
        return OptionalRef<PointerValueRef>(toRef(toImpl(this)->target().value()));
    }
    return nullptr;
}

FinalizationRegistryObjectRef* FinalizationRegistryObjectRef::create(ExecutionStateRef* state, ObjectRef* cleanupCallback, ContextRef* realm)
{
    return toRef(new FinalizationRegistryObject(*toImpl(state), toImpl(cleanupCallback), toImpl(realm)));
}


#if defined(ENABLE_EXTENDED_API)
void TemplateRef::set(ValueRef* propertyName, ValueRef* data, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    toImpl(this)->set(toImpl(propertyName), toImpl(data), isWritable, isEnumerable, isConfigurable);
}

void TemplateRef::set(ValueRef* propertyName, TemplateRef* data, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    toImpl(this)->set(toImpl(propertyName), toImpl(data), isWritable, isEnumerable, isConfigurable);
}

void TemplateRef::setAccessorProperty(ValueRef* propertyName, OptionalRef<FunctionTemplateRef> getter, OptionalRef<FunctionTemplateRef> setter, bool isEnumerable, bool isConfigurable)
{
    Optional<FunctionTemplate*> getterImpl(toImpl(getter.get()));
    Optional<FunctionTemplate*> setterImpl(toImpl(setter.get()));

    toImpl(this)->setAccessorProperty(toImpl(propertyName), getterImpl, setterImpl, isEnumerable, isConfigurable);
}

void TemplateRef::setNativeDataAccessorProperty(ValueRef* propertyName, ObjectRef::NativeDataAccessorPropertyGetter getter, ObjectRef::NativeDataAccessorPropertySetter setter,
                                                bool isWritable, bool isEnumerable, bool isConfigurable, bool actsLikeJSGetterSetter)
{
    ObjectRef::NativeDataAccessorPropertyData* publicData = new ObjectRef::NativeDataAccessorPropertyData(isWritable, isEnumerable, isConfigurable, getter, setter);

    ObjectPropertyNativeGetterSetterData* innerData = new ObjectPropertyNativeGetterSetterData(isWritable, isEnumerable, isConfigurable, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
        ObjectRef::NativeDataAccessorPropertyData* publicData = reinterpret_cast<ObjectRef::NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
        return toImpl(publicData->m_getter(toRef(&state), toRef(self), toRef(receiver), publicData));
    },
                                                                                               nullptr, actsLikeJSGetterSetter);

    if (!isWritable) {
        innerData->m_setter = nullptr;
    } else if (publicData->m_isWritable && !publicData->m_setter) {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            return false;
        };
    } else {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            ObjectRef::NativeDataAccessorPropertyData* publicData = reinterpret_cast<ObjectRef::NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
            return publicData->m_setter(toRef(&state), toRef(self), toRef(receiver), publicData, toRef(setterInputData));
        };
    }

    toImpl(this)->setNativeDataAccessorProperty(toImpl(propertyName), innerData, publicData);
}

void TemplateRef::setNativeDataAccessorProperty(ValueRef* propertyName, ObjectRef::NativeDataAccessorPropertyData* publicData, bool actsLikeJSGetterSetter)
{
    ObjectPropertyNativeGetterSetterData* innerData = new ObjectPropertyNativeGetterSetterData(publicData->m_isWritable, publicData->m_isEnumerable, publicData->m_isConfigurable,
                                                                                               [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                   ObjectRef::NativeDataAccessorPropertyData* publicData = reinterpret_cast<ObjectRef::NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
                                                                                                   return toImpl(publicData->m_getter(toRef(&state), toRef(self), toRef(receiver), publicData));
                                                                                               },
                                                                                               nullptr);
    innerData->m_actsLikeJSGetterSetter = actsLikeJSGetterSetter;
    if (!publicData->m_isWritable) {
        innerData->m_setter = nullptr;
    } else if (publicData->m_isWritable && !publicData->m_setter) {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            return false;
        };
    } else {
        innerData->m_setter = [](ExecutionState& state, Object* self, const Value& receiver, EncodedValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            ObjectRef::NativeDataAccessorPropertyData* publicData = reinterpret_cast<ObjectRef::NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
            return publicData->m_setter(toRef(&state), toRef(self), toRef(receiver), publicData, toRef(setterInputData));
        };
    }

    toImpl(this)->setNativeDataAccessorProperty(toImpl(propertyName), innerData, publicData);
}

bool TemplateRef::has(ValueRef* propertyName)
{
    return toImpl(this)->has(toImpl(propertyName));
}

bool TemplateRef::remove(ValueRef* propertyName)
{
    return toImpl(this)->remove(toImpl(propertyName));
}

ObjectRef* TemplateRef::instantiate(ContextRef* ctx)
{
    return toRef(toImpl(this)->instantiate(toImpl(ctx)));
}

bool TemplateRef::didInstantiate()
{
    return toImpl(this)->didInstantiate();
}

bool TemplateRef::isObjectTemplate()
{
    return toImpl(this)->isObjectTemplate();
}

bool TemplateRef::isFunctionTemplate()
{
    return toImpl(this)->isFunctionTemplate();
}

void TemplateRef::setInstanceExtraData(void* ptr)
{
    toImpl(this)->setInstanceExtraData(ptr);
}

void* TemplateRef::instanceExtraData()
{
    return toImpl(this)->instanceExtraData();
}

ObjectTemplateRef* ObjectTemplateRef::create()
{
    return toRef(new ObjectTemplate());
}

void ObjectTemplateRef::setNamedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data)
{
    toImpl(this)->setNamedPropertyHandler(data);
}

void ObjectTemplateRef::setIndexedPropertyHandler(const ObjectTemplatePropertyHandlerConfiguration& data)
{
    toImpl(this)->setIndexedPropertyHandler(data);
}

void ObjectTemplateRef::removeNamedPropertyHandler()
{
    toImpl(this)->removeNamedPropertyHandler();
}

void ObjectTemplateRef::removeIndexedPropertyHandler()
{
    toImpl(this)->removeIndexedPropertyHandler();
}

OptionalRef<FunctionTemplateRef> ObjectTemplateRef::constructor()
{
    if (toImpl(this)->constructor()) {
        return toRef(toImpl(this)->constructor().value());
    } else {
        return nullptr;
    }
}

bool ObjectTemplateRef::installTo(ContextRef* ctx, ObjectRef* target)
{
    return toImpl(this)->installTo(toImpl(ctx), toImpl(target));
}

FunctionTemplateRef* FunctionTemplateRef::create(AtomicStringRef* name, size_t argumentCount, bool isStrict, bool isConstructor,
                                                 FunctionTemplateRef::NativeFunctionPointer fn)
{
    return toRef(new FunctionTemplate(toImpl(name), argumentCount, isStrict, isConstructor, fn));
}

void FunctionTemplateRef::setName(AtomicStringRef* name)
{
    toImpl(this)->setName(toImpl(name));
}

void FunctionTemplateRef::setLength(size_t length)
{
    toImpl(this)->setLength(length);
}

void FunctionTemplateRef::updateCallbackFunction(FunctionTemplateRef::NativeFunctionPointer fn)
{
    toImpl(this)->updateCallbackFunction(fn);
}

ObjectTemplateRef* FunctionTemplateRef::prototypeTemplate()
{
    return toRef(toImpl(this)->prototypeTemplate());
}

ObjectTemplateRef* FunctionTemplateRef::instanceTemplate()
{
    return toRef(toImpl(this)->instanceTemplate());
}

void FunctionTemplateRef::inherit(OptionalRef<FunctionTemplateRef> parent)
{
    toImpl(this)->inherit(toImpl(parent.value()));
}

OptionalRef<FunctionTemplateRef> FunctionTemplateRef::parent()
{
    if (toImpl(this)->parent()) {
        return toRef(toImpl(this)->parent().value());
    } else {
        return nullptr;
    }
}
#endif

ScriptParserRef::InitializeScriptResult::InitializeScriptResult()
    : script()
    , parseErrorMessage(StringRef::emptyString())
    , parseErrorCode(ErrorObjectRef::Code::None)
{
}

ScriptRef* ScriptParserRef::InitializeScriptResult::fetchScriptThrowsExceptionIfParseError(ExecutionStateRef* state)
{
    if (!script.hasValue()) {
        ErrorObject::throwBuiltinError(*toImpl(state), (ErrorCode)parseErrorCode, toImpl(parseErrorMessage)->toNonGCUTF8StringData().data());
    }

    return script.value();
}

ScriptParserRef::InitializeFunctionScriptResult::InitializeFunctionScriptResult()
    : script()
    , functionObject()
    , parseErrorMessage(StringRef::emptyString())
    , parseErrorCode(ErrorObjectRef::Code::None)
{
}

ScriptParserRef::InitializeScriptResult ScriptParserRef::initializeScript(StringRef* source, StringRef* srcName, bool isModule)
{
    auto internalResult = toImpl(this)->initializeScript(toImpl(source), toImpl(srcName), isModule);
    ScriptParserRef::InitializeScriptResult result;
    if (internalResult.script) {
        result.script = toRef(internalResult.script.value());
    } else {
        result.parseErrorMessage = toRef(internalResult.parseErrorMessage);
        result.parseErrorCode = (Escargot::ErrorObjectRef::Code)internalResult.parseErrorCode;
    }

    return result;
}

ScriptParserRef::InitializeFunctionScriptResult ScriptParserRef::initializeFunctionScript(StringRef* sourceName, AtomicStringRef* functionName, size_t argumentCount, ValueRef** argumentNameArray, ValueRef* functionBody)
{
    // temporal ExecutionState
    ExecutionState state(toImpl(this)->context());

    Value* argArray = ALLOCA(sizeof(Value) * argumentCount, Value);
    for (size_t i = 0; i < argumentCount; i++) {
        argArray[i] = toImpl(argumentNameArray[i]);
    }

    auto internalResult = FunctionObject::createFunctionScript(state, toImpl(sourceName), toImpl(functionName), argumentCount, argArray, toImpl(functionBody), false);

    ScriptParserRef::InitializeFunctionScriptResult result;

    if (internalResult.script) {
        Script* script = internalResult.script.value();
        InterpretedCodeBlock* codeBlock = script->topCodeBlock()->childBlockAt(0);

        // create FunctionObject
        LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, script->topCodeBlock(), state.context()->globalObject(), state.context()->globalDeclarativeRecord(), state.context()->globalDeclarativeStorage()), nullptr);
        Object* proto = state.context()->globalObject()->functionPrototype();
        ScriptFunctionObject* func = new ScriptFunctionObject(state, proto, codeBlock, globalEnvironment, true, false);

        result.script = toRef(internalResult.script.value());
        result.functionObject = toRef(func);
    } else {
        result.parseErrorMessage = toRef(internalResult.parseErrorMessage);
        result.parseErrorCode = (Escargot::ErrorObjectRef::Code)internalResult.parseErrorCode;
    }

    return result;
}

ScriptParserRef::InitializeScriptResult ScriptParserRef::initializeJSONModule(StringRef* sourceCode, StringRef* srcName)
{
    ScriptParserRef::InitializeScriptResult result;

    result.script = toRef(toImpl(this)->initializeJSONModule(toImpl(sourceCode), toImpl(srcName)));
    return result;
}

bool ScriptRef::isModule()
{
    return toImpl(this)->isModule();
}

bool ScriptRef::isExecuted()
{
    return toImpl(this)->isExecuted();
}

StringRef* ScriptRef::src()
{
    return toRef(toImpl(this)->srcName());
}

StringRef* ScriptRef::sourceCode()
{
    return toRef(toImpl(this)->sourceCode());
}

ContextRef* ScriptRef::context()
{
    return toRef(toImpl(this)->context());
}

ValueRef* ScriptRef::execute(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->execute(*toImpl(state)));
}

size_t ScriptRef::moduleRequestsLength()
{
    return toImpl(this)->moduleRequestsLength();
}

StringRef* ScriptRef::moduleRequest(size_t i)
{
    return toRef(toImpl(this)->moduleRequest(i));
}

ObjectRef* ScriptRef::moduleNamespace(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->getModuleNamespace(*toImpl(state))->asObject());
}

bool ScriptRef::wasThereErrorOnModuleEvaluation()
{
    return toImpl(this)->wasThereErrorOnModuleEvaluation();
}

ValueRef* ScriptRef::moduleEvaluationError()
{
    return toRef(toImpl(this)->moduleEvaluationError());
}

ScriptRef::ModuleStatus ScriptRef::moduleStatus()
{
    auto md = toImpl(this)->moduleData();
    if (md->m_evaluationError) {
        return ModuleStatus::Errored;
    }
    switch (md->m_status) {
    case Script::ModuleData::Unlinked:
        return ModuleStatus::Uninstantiated;
    case Script::ModuleData::Linking:
        return ModuleStatus::Instantiating;
    case Script::ModuleData::Linked:
        return ModuleStatus::Instantiated;
    case Script::ModuleData::Evaluating:
        return ModuleStatus::Evaluating;
    case Script::ModuleData::Evaluated:
        return ModuleStatus::Evaluated;
    }

    ASSERT_NOT_REACHED();
    return ModuleStatus::Errored;
}

ValueRef* ScriptRef::moduleInstantiate(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->moduleInstantiate(*toImpl(state)));
}

ValueRef* ScriptRef::moduleEvaluate(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->moduleEvaluate(*toImpl(state)));
}

PlatformRef::LoadModuleResult::LoadModuleResult(ScriptRef* result)
    : script(result)
    , errorMessage(StringRef::emptyString())
    , errorCode(ErrorObjectRef::Code::None)
{
}

PlatformRef::LoadModuleResult::LoadModuleResult(ErrorObjectRef::Code errorCode, StringRef* errorMessage)
    : script(nullptr)
    , errorMessage(errorMessage)
    , errorCode(errorCode)
{
}

bool PlatformRef::isCustomLoggingEnabled()
{
#if defined(ENABLE_CUSTOM_LOGGING)
    return true;
#else
    return false;
#endif
}

bool SerializerRef::serializeInto(ValueRef* value, std::ostringstream& output)
{
    return Serializer::serializeInto(toImpl(value), output);
}

ValueRef* SerializerRef::deserializeFrom(ContextRef* context, std::istringstream& input)
{
    std::unique_ptr<SerializedValue> value = Serializer::deserializeFrom(input);

    SandBox sb(toImpl(context));
    auto result = sb.run([](ExecutionState& state, void* data) -> Value {
        std::unique_ptr<SerializedValue>* value = (std::unique_ptr<SerializedValue>*)data;
        return value->get()->toValue(state);
    },
                         &value);

    ASSERT(result.error.isEmpty());
    return toRef(result.result);
}

bool WASMOperationsRef::isWASMOperationsEnabled()
{
#if defined(ENABLE_WASM)
    return true;
#else
    return false;
#endif
}

#if defined(ENABLE_WASM)
ValueRef* WASMOperationsRef::copyStableBufferBytes(ExecutionStateRef* state, ValueRef* source)
{
    return toRef(WASMOperations::copyStableBufferBytes(*toImpl(state), toImpl(source)));
}

ObjectRef* WASMOperationsRef::asyncCompileModule(ExecutionStateRef* state, ValueRef* source)
{
    return toRef(WASMOperations::asyncCompileModule(*toImpl(state), toImpl(source)));
}
ObjectRef* WASMOperationsRef::instantiatePromiseOfModuleWithImportObject(ExecutionStateRef* state, PromiseObjectRef* promiseOfModule, ValueRef* importObj)
{
    return toRef(WASMOperations::instantiatePromiseOfModuleWithImportObject(*toImpl(state), toImpl(promiseOfModule), toImpl(importObj)));
}
#else
ValueRef* WASMOperationsRef::copyStableBufferBytes(ExecutionStateRef* state, ValueRef* source)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable WASM");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

ObjectRef* WASMOperationsRef::asyncCompileModule(ExecutionStateRef* state, ValueRef* source)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable WASM");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

ObjectRef* WASMOperationsRef::instantiatePromiseOfModuleWithImportObject(ExecutionStateRef* state, PromiseObjectRef* promiseOfModule, ValueRef* importObj)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable WASM");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

} // namespace Escargot
