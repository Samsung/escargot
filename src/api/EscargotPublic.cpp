/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include <cstdlib> // size_t
#include "GCUtil.h"
#include "util/Vector.h"
#include "EscargotPublic.h"
#include "parser/ast/Node.h"
#include "parser/ScriptParser.h"
#include "parser/CodeBlock.h"
#include "runtime/Context.h"
#include "runtime/FunctionObject.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "runtime/SandBox.h"
#include "runtime/Environment.h"
#include "runtime/SymbolObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/DateObject.h"
#include "runtime/StringObject.h"
#include "runtime/NumberObject.h"
#include "runtime/BooleanObject.h"
#include "runtime/RegExpObject.h"
#include "runtime/Job.h"
#include "runtime/JobQueue.h"
#include "runtime/PromiseObject.h"
#include "runtime/ProxyObject.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/SetObject.h"
#include "runtime/WeakSetObject.h"
#include "runtime/MapObject.h"
#include "runtime/WeakMapObject.h"
#include "runtime/CompressibleString.h"

namespace Escargot {

#define DEFINE_CAST(ClassName)                       \
    inline ClassName* toImpl(ClassName##Ref* v)      \
    {                                                \
        return reinterpret_cast<ClassName*>(v);      \
    }                                                \
    inline ClassName##Ref* toRef(ClassName* v)       \
    {                                                \
        return reinterpret_cast<ClassName##Ref*>(v); \
    }

DEFINE_CAST(VMInstance);
DEFINE_CAST(Context);
DEFINE_CAST(ExecutionState);
DEFINE_CAST(String);
DEFINE_CAST(Symbol);
DEFINE_CAST(PointerValue);
DEFINE_CAST(Object);
DEFINE_CAST(IteratorObject);
DEFINE_CAST(ArrayObject)
DEFINE_CAST(StringObject)
DEFINE_CAST(SymbolObject)
DEFINE_CAST(NumberObject)
DEFINE_CAST(BooleanObject)
DEFINE_CAST(RegExpObject)
DEFINE_CAST(ErrorObject)
DEFINE_CAST(ReferenceErrorObject);
DEFINE_CAST(TypeErrorObject);
DEFINE_CAST(SyntaxErrorObject);
DEFINE_CAST(RangeErrorObject);
DEFINE_CAST(URIErrorObject);
DEFINE_CAST(EvalErrorObject);
DEFINE_CAST(GlobalObject);
DEFINE_CAST(FunctionObject);
DEFINE_CAST(DateObject);
DEFINE_CAST(PromiseObject);
DEFINE_CAST(ProxyObject);
DEFINE_CAST(Job);
DEFINE_CAST(Script);
DEFINE_CAST(ScriptParser);
DEFINE_CAST(ArrayBufferObject);
DEFINE_CAST(ArrayBufferView);
DEFINE_CAST(Int8ArrayObject);
DEFINE_CAST(Uint8ArrayObject);
DEFINE_CAST(Int16ArrayObject);
DEFINE_CAST(Uint16ArrayObject);
DEFINE_CAST(Int32ArrayObject);
DEFINE_CAST(Uint32ArrayObject);
DEFINE_CAST(Uint8ClampedArrayObject);
DEFINE_CAST(Float32ArrayObject);
DEFINE_CAST(Float64ArrayObject);
DEFINE_CAST(SetObject);
DEFINE_CAST(WeakSetObject);
DEFINE_CAST(MapObject);
DEFINE_CAST(WeakMapObject);

#undef DEFINE_CAST

inline ValueRef* toRef(const Value& v)
{
    ASSERT(!v.isEmpty());
    return reinterpret_cast<ValueRef*>(SmallValue(v).payload());
}

inline Value toImpl(const ValueRef* v)
{
    ASSERT(v);
    return Value(SmallValue::fromPayload(v));
}

inline OptionalRef<ValueRef> toOptionalValue(const Value& v)
{
    return OptionalRef<ValueRef>(reinterpret_cast<ValueRef*>(SmallValue(v).payload()));
}

inline Value toImpl(const OptionalRef<ValueRef>& v)
{
    if (LIKELY(v.hasValue())) {
        return Value(SmallValue::fromPayload(v.value()));
    }
    return Value(Value::EmptyValue);
}

inline ValueVectorRef* toRef(const SmallValueVector* v)
{
    return reinterpret_cast<ValueVectorRef*>(const_cast<SmallValueVector*>(v));
}

inline SmallValueVector* toImpl(ValueVectorRef* v)
{
    return reinterpret_cast<SmallValueVector*>(v);
}

inline AtomicStringRef* toRef(const AtomicString& v)
{
    return reinterpret_cast<AtomicStringRef*>(v.string());
}

inline AtomicString toImpl(AtomicStringRef* v)
{
    return AtomicString::fromPayload(reinterpret_cast<void*>(v));
}

void Globals::initialize()
{
    Heap::initialize();
}

void Globals::finalize()
{
    Heap::finalize();
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
        GC_REGISTER_FINALIZER_NO_ORDER(ptr, [](void* obj,
                                               void* data) {
            ((GCAllocatedMemoryFinalizer)data)(obj);
        },
                                       (void*)callback, nullptr, nullptr);
    } else {
        GC_REGISTER_FINALIZER_NO_ORDER(ptr, nullptr, nullptr, nullptr, nullptr);
    }
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

static Memory::OnGCEventListener g_gcEventListener;
static void gcEventListener(GC_EventType evtType, void*)
{
    if (GC_EVENT_RECLAIM_END == evtType && g_gcEventListener) {
        g_gcEventListener();
    }
}

void Memory::setGCEventListener(OnGCEventListener l)
{
    g_gcEventListener = l;
    GC_remove_event_callback(gcEventListener, nullptr);
    GC_add_event_callback(gcEventListener, nullptr);
}

// I store ref count as SmallValue. this can prevent what bdwgc can see ref count as address (SmallValue store integer value as odd)
using PersistentValueRefMapImpl = std::unordered_map<ValueRef*, SmallValue, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<std::pair<ValueRef* const, SmallValue>>>;

PersistentRefHolder<PersistentValueRefMap> PersistentValueRefMap::create()
{
    return PersistentRefHolder<PersistentValueRefMap>((PersistentValueRefMap*)new (Memory::gcMalloc(sizeof(PersistentValueRefMapImpl))) PersistentValueRefMapImpl());
}

uint32_t PersistentValueRefMap::add(ValueRef* ptr)
{
    auto value = SmallValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return 0;
    }

    PersistentValueRefMapImpl* self = (PersistentValueRefMapImpl*)this;
    auto iter = self->find(ptr);
    if (iter == self->end()) {
        self->insert(std::make_pair(ptr, SmallValue(1)));
        return 1;
    } else {
        iter->second = SmallValue(iter->second.asUint32() + 1);
        return iter->second.asUint32();
    }
}

uint32_t PersistentValueRefMap::remove(ValueRef* ptr)
{
    auto value = SmallValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return 0;
    }

    PersistentValueRefMapImpl* self = (PersistentValueRefMapImpl*)this;
    auto iter = self->find(ptr);
    if (iter == self->end()) {
        return 0;
    } else {
        if (iter->second.asUint32() == 1) {
            self->erase(iter);
            return 0;
        } else {
            iter->second = SmallValue(iter->second.asUint32() - 1);
            return iter->second.asUint32();
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
    return toRef(new ASCIIString(s, len));
}

StringRef* StringRef::createFromUTF8(const char* s, size_t len)
{
    return toRef(String::fromUTF8(s, len));
}

StringRef* StringRef::createFromUTF16(const char16_t* s, size_t len)
{
    return toRef(new UTF16String(s, len));
}

StringRef* StringRef::createFromLatin1(const unsigned char* s, size_t len)
{
    return toRef(new Latin1String(s, len));
}

StringRef* StringRef::createExternalFromASCII(const char* s, size_t len)
{
    return toRef(new ASCIIString(s, len, String::FromExternalMemory));
}

StringRef* StringRef::createExternalFromLatin1(const unsigned char* s, size_t len)
{
    return toRef(new Latin1String(s, len, String::FromExternalMemory));
}

StringRef* StringRef::createExternalFromUTF16(const char16_t* s, size_t len)
{
    return toRef(new UTF16String(s, len, String::FromExternalMemory));
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
StringRef* StringRef::createFromUTF8ToCompressibleString(ContextRef* context, const char* s, size_t len)
{
    return toRef(String::fromUTF8ToCompressibleString(toImpl(context), s, len));
}

StringRef* StringRef::createFromUTF16ToCompressibleString(ContextRef* context, const char16_t* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(context), s, len));
}

StringRef* StringRef::createFromASCIIToCompressibleString(ContextRef* context, const char* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(context), s, len));
}

StringRef* StringRef::createFromLatin1ToCompressibleString(ContextRef* context, const unsigned char* s, size_t len)
{
    return toRef(new CompressibleString(toImpl(context), s, len));
}

void* StringRef::allocateStringDataBufferForCompressibleString(size_t byteLength)
{
    return CompressibleString::allocateStringDataBuffer(byteLength);
}

void StringRef::deallocateStringDataBufferForCompressibleString(void* ptr)
{
    CompressibleString::deallocateStringDataBuffer(ptr);
}

StringRef* StringRef::createFromAlreadyAllocatedBufferToCompressibleString(ContextRef* context, void* buffer, size_t stringLen, bool is8Bit)
{
    return toRef(new CompressibleString(toImpl(context), buffer, stringLen, is8Bit));
}

#else
StringRef* StringRef::createFromUTF8ToCompressibleString(ContextRef* context, const char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createCompressibleString(ContextRef* context, const unsigned char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromASCIIToCompressibleString(ContextRef* context, const char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromLatin1ToCompressibleString(ContextRef* context, const unsigned char* s, size_t len)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void* StringRef::allocateStringDataBufferForCompressibleString(size_t byteLength)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void StringRef::deallocateStringDataBufferForCompressibleString(void* ptr)
{
    ESCARGOT_LOG_ERROR("If you want to use this function, you should enable source compression");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

StringRef* StringRef::createFromAlreadyAllocatedBufferToCompressibleString(ContextRef* context, void* buffer, size_t stringLen, bool is8Bit)
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

bool StringRef::equals(StringRef* src)
{
    return toImpl(this)->equals(toImpl(src));
}

StringRef* StringRef::substring(size_t from, size_t to)
{
    return toRef(toImpl(this)->substring(from, to));
}

std::string StringRef::toStdUTF8String()
{
    return toImpl(this)->toNonGCUTF8StringData();
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

SymbolRef* SymbolRef::create(StringRef* desc)
{
    return toRef(new Symbol(toImpl(desc)));
}

SymbolRef* SymbolRef::fromGlobalSymbolRegistry(VMInstanceRef* vm, StringRef* desc)
{
    return toRef(Symbol::fromGlobalSymbolRegistry(toImpl(vm), toImpl(desc)));
}

StringRef* SymbolRef::description()
{
    return toRef(toImpl(this)->description());
}

StringRef* SymbolRef::symbolDescriptiveString()
{
    return toRef(toImpl(this)->symbolDescriptiveString());
}

#define DEFINE_IS_POINTERVALUE_XXX(XXX) \
    bool ValueRef::is##XXX() { return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->is##XXX(); }
#define DEFINE_AS_POINTERVALUE_XXX(XXX) \
    XXX##Ref* ValueRef::as##XXX() { return toRef(toImpl(this).asPointerValue()->as##XXX()); }
#define DEFINE_IS_AS_POINTERVALUE_XXX(XXX) \
    DEFINE_IS_POINTERVALUE_XXX(XXX)        \
    DEFINE_AS_POINTERVALUE_XXX(XXX)

DEFINE_IS_AS_POINTERVALUE_XXX(String)
DEFINE_IS_AS_POINTERVALUE_XXX(Symbol)
DEFINE_IS_AS_POINTERVALUE_XXX(Object)
DEFINE_IS_AS_POINTERVALUE_XXX(FunctionObject)
DEFINE_IS_AS_POINTERVALUE_XXX(ArrayObject)
DEFINE_IS_AS_POINTERVALUE_XXX(StringObject)
DEFINE_IS_AS_POINTERVALUE_XXX(SymbolObject)
DEFINE_IS_AS_POINTERVALUE_XXX(NumberObject)
DEFINE_IS_AS_POINTERVALUE_XXX(BooleanObject)
DEFINE_IS_AS_POINTERVALUE_XXX(RegExpObject)
DEFINE_IS_AS_POINTERVALUE_XXX(DateObject)
DEFINE_IS_AS_POINTERVALUE_XXX(GlobalObject)
DEFINE_IS_AS_POINTERVALUE_XXX(ErrorObject)
DEFINE_IS_AS_POINTERVALUE_XXX(ArrayBufferObject)
DEFINE_IS_AS_POINTERVALUE_XXX(ArrayBufferView)

bool ValueRef::isArrayPrototypeObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isArrayPrototypeObject();
}

bool ValueRef::isTypedArrayObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isTypedArrayObject();
}

bool ValueRef::isTypedArrayPrototypeObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isTypedArrayPrototypeObject();
}

bool ValueRef::isDataViewObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isDataViewObject();
}

#define DEFINE_TYPEDARRAY_IMPL(TypeName)                                                                                                                                                                \
    bool ValueRef::is##TypeName##ArrayObject()                                                                                                                                                          \
    {                                                                                                                                                                                                   \
        return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isArrayBufferView() && toImpl(this).asPointerValue()->asArrayBufferView()->typedArrayType() == TypedArrayType::TypeName; \
    }                                                                                                                                                                                                   \
                                                                                                                                                                                                        \
    TypeName##ArrayObjectRef* ValueRef::as##TypeName##ArrayObject()                                                                                                                                     \
    {                                                                                                                                                                                                   \
        return toRef((TypeName##ArrayObject*)(toImpl(this).asPointerValue()->asArrayBufferView()));                                                                                                     \
    }

DEFINE_TYPEDARRAY_IMPL(Int8);
DEFINE_TYPEDARRAY_IMPL(Uint8);
DEFINE_TYPEDARRAY_IMPL(Int16);
DEFINE_TYPEDARRAY_IMPL(Uint16);
DEFINE_TYPEDARRAY_IMPL(Int32);
DEFINE_TYPEDARRAY_IMPL(Uint32);
DEFINE_TYPEDARRAY_IMPL(Uint8Clamped);
DEFINE_TYPEDARRAY_IMPL(Float32);
DEFINE_TYPEDARRAY_IMPL(Float64);


DEFINE_IS_AS_POINTERVALUE_XXX(PromiseObject)
DEFINE_IS_AS_POINTERVALUE_XXX(ProxyObject)

bool ValueRef::isArgumentsObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isArgumentsObject();
}

bool ValueRef::isGeneratorFunctionObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isFunctionObject() && toImpl(this).asPointerValue()->asFunctionObject()->isGenerator();
}

bool ValueRef::isGeneratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isGeneratorObject();
}

DEFINE_IS_AS_POINTERVALUE_XXX(SetObject)
DEFINE_IS_AS_POINTERVALUE_XXX(WeakSetObject)
DEFINE_IS_AS_POINTERVALUE_XXX(MapObject)
DEFINE_IS_AS_POINTERVALUE_XXX(WeakMapObject)

bool ValueRef::isSetIteratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isSetIteratorObject();
}

bool ValueRef::isMapIteratorObject()
{
    return toImpl(this).isPointerValue() && toImpl(this).asPointerValue()->isMapIteratorObject();
}

ValueRef* ValueRef::call(ExecutionStateRef* state, ValueRef* receiver, const size_t argc, ValueRef** argv)
{
    PointerValue* o = toImpl(this).asPointerValue();
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value, state);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(Object::call(*toImpl(state), o, toImpl(receiver), argc, newArgv));
}

ObjectRef* ValueRef::construct(ExecutionStateRef* state, const size_t argc, ValueRef** argv)
{
    PointerValue* o = toImpl(this).asPointerValue();
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value, state);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(Object::construct(*toImpl(state), o, argc, newArgv));
}

class PlatformBridge : public Platform {
public:
    PlatformBridge(PlatformRef* p)
        : m_platform(p)
    {
    }

    virtual void* onArrayBufferObjectDataBufferMalloc(Context* whereObjectMade, ArrayBufferObject* obj, size_t sizeInByte) override
    {
        return m_platform->onArrayBufferObjectDataBufferMalloc(toRef(whereObjectMade), toRef(obj), sizeInByte);
    }

    virtual void* onSharedArrayBufferObjectDataBufferMalloc(Context* whereObjectMade, ArrayBufferObject* obj, size_t sizeInByte) override
    {
        return m_platform->onSharedArrayBufferObjectDataBufferMalloc(toRef(whereObjectMade), toRef(obj), sizeInByte);
    }

    virtual void onArrayBufferObjectDataBufferFree(Context* whereObjectMade, ArrayBufferObject* obj, void* buffer) override
    {
        m_platform->onArrayBufferObjectDataBufferFree(toRef(whereObjectMade), toRef(obj), buffer);
    }

    virtual void onSharedArrayBufferObjectDataBufferFree(Context* whereObjectMade, ArrayBufferObject* obj, void* buffer) override
    {
        m_platform->onSharedArrayBufferObjectDataBufferFree(toRef(whereObjectMade), toRef(obj), buffer);
    }

    virtual void didPromiseJobEnqueued(Context* relatedContext, PromiseObject* obj) override
    {
        m_platform->didPromiseJobEnqueued(toRef(relatedContext), toRef(obj));
    }

    virtual LoadModuleResult onLoadModule(Context* relatedContext, Script* whereRequestFrom, String* moduleSrc) override
    {
        LoadModuleResult result;
        auto refResult = m_platform->onLoadModule(toRef(relatedContext), toRef(whereRequestFrom), toRef(moduleSrc));

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

    PlatformRef* m_platform;
};

Evaluator::StackTraceData::StackTraceData()
    : src(toRef(String::emptyString))
    , sourceCode(toRef(String::emptyString))
    , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
    , functionName(toRef(String::emptyString))
    , isFunction(false)
    , isConstructor(false)
    , isAssociatedWithJavaScriptCode(false)
    , isEval(false)
{
}

Evaluator::EvaluatorResult::EvaluatorResult()
    : result()
    , error()
{
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
        new (&r.stackTraceData) GCManagedVector<Evaluator::StackTraceData>(result.stackTraceData.size());
        for (size_t i = 0; i < result.stackTraceData.size(); i++) {
            Evaluator::StackTraceData t;
            t.src = toRef(result.stackTraceData[i].src);
            t.sourceCode = toRef(result.stackTraceData[i].sourceCode);
            t.loc.index = result.stackTraceData[i].loc.index;
            t.loc.line = result.stackTraceData[i].loc.line;
            t.loc.column = result.stackTraceData[i].loc.column;
            t.functionName = toRef(result.stackTraceData[i].functionName);
            t.isFunction = result.stackTraceData[i].isFunction;
            t.isConstructor = result.stackTraceData[i].isConstructor;
            t.isAssociatedWithJavaScriptCode = result.stackTraceData[i].isAssociatedWithJavaScriptCode;
            t.isEval = result.stackTraceData[i].isEval;
            r.stackTraceData[i] = t;
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
        ValueRef* (*runner)(ExecutionStateRef * state, void* passedData) = (ValueRef * (*)(ExecutionStateRef * state, void* passedData))sender->fn;
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
        ValueRef* (*runner)(ExecutionStateRef * state, void* passedData, void* passedData2) = (ValueRef * (*)(ExecutionStateRef * state, void* passedData, void* passedData2))sender->fn;
        return toImpl(runner(toRef(&state), sender->data, sender->data2));
    },
                         &sender);
    return toEvaluatorResultRef(result);
}

PersistentRefHolder<VMInstanceRef> VMInstanceRef::create(PlatformRef* platform, const char* locale, const char* timezone)
{
    return PersistentRefHolder<VMInstanceRef>(toRef(new VMInstance(new PlatformBridge(platform), locale, timezone)));
}

PlatformRef* VMInstanceRef::platform()
{
    return ((PlatformBridge*)toImpl(this))->m_platform;
}

void VMInstanceRef::setOnVMInstanceDelete(OnVMInstanceDelete cb)
{
    toImpl(this)->setOnDestroyCallback([](VMInstance* instance, void* data) {
        if (data) {
            ((OnVMInstanceDelete)data)(toRef(instance));
        }
    },
                                       (void*)cb);
}

void VMInstanceRef::clearCachesRelatedWithContext()
{
    VMInstance* imp = toImpl(this);
    imp->m_regexpCache->clear();
    imp->m_cachedUTC = nullptr;
    imp->globalSymbolRegistry().clear();
}

#define DECLARE_GLOBAL_SYMBOLS(name)                      \
    SymbolRef* VMInstanceRef::name##Symbol()              \
    {                                                     \
        return toRef(toImpl(this)->globalSymbols().name); \
    }
DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

bool VMInstanceRef::hasPendingPromiseJob()
{
    return toImpl(this)->hasPendingPromiseJob();
}

Evaluator::EvaluatorResult VMInstanceRef::executePendingPromiseJob()
{
    auto result = toImpl(this)->executePendingPromiseJob();
    return toEvaluatorResultRef(result);
}

PersistentRefHolder<ContextRef> ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = toImpl(vminstanceref);
    return PersistentRefHolder<ContextRef>(toRef(new Context(vminstance)));
}

void ContextRef::clearRelatedQueuedPromiseJobs()
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
    return toRef(new SmallValueVector(size));
}

size_t ValueVectorRef::size()
{
    return toImpl(this)->size();
}

void ValueVectorRef::pushBack(ValueRef* val)
{
    toImpl(this)->pushBack(SmallValue::fromPayload(val));
}

void ValueVectorRef::insert(size_t pos, ValueRef* val)
{
    toImpl(this)->insert(pos, SmallValue::fromPayload(val));
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
    toImpl(this)->data()[idx] = SmallValue::fromPayload(newValue);
}

void ValueVectorRef::resize(size_t newSize)
{
    toImpl(this)->resize(newSize);
}

ObjectRef* ObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new Object(*toImpl(state)));
}


// can not redefine or delete virtual property
class ExposableObject : public Object {
public:
    ExposableObject(ExecutionState& state, ExposableObjectGetOwnPropertyCallback getOwnPropetyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback, ExposableObjectEnumerationCallback enumerationCallback, ExposableObjectDeleteOwnPropertyCallback deleteOwnPropertyCallback)
        : Object(state)
        , m_getOwnPropetyCallback(getOwnPropetyCallback)
        , m_defineOwnPropertyCallback(defineOwnPropertyCallback)
        , m_enumerationCallback(enumerationCallback)
        , m_deleteOwnPropertyCallback(deleteOwnPropertyCallback)
    {
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value PV = P.toPlainValue(state);
        if (!PV.isSymbol()) {
            auto result = m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(PV));
            if (result.m_value.hasValue()) {
                return ObjectGetResult(toImpl(result.m_value.value()), result.m_isWritable, result.m_isEnumerable, result.m_isConfigurable);
            }
        }
        return Object::getOwnProperty(state, P);
    }
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        // Only value type supported
        if (desc.isValuePresent()) {
            Value PV = P.toPlainValue(state);
            if (!PV.isSymbol() && m_defineOwnPropertyCallback(toRef(&state), toRef(this), toRef(PV), toRef(desc.value()))) {
                return true;
            }
        }
        return Object::defineOwnProperty(state, P, desc);
    }
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value PV = P.toPlainValue(state);
        if (!PV.isSymbol()) {
            auto result = m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toPlainValue(state)));
            if (result.m_value.hasValue()) {
                return m_deleteOwnPropertyCallback(toRef(&state), toRef(this), toRef(P.toPlainValue(state)));
            }
        }
        return Object::deleteOwnProperty(state, P);
    }
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey = true) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
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

    virtual bool isInlineCacheable()
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
    ObjectGetResult desc = toImpl(this)->getOwnProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), name));
    return toRef(desc.toPropertyDescriptor(*toImpl(state), toImpl(this)));
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

bool ObjectRef::defineNativeDataAccessorProperty(ExecutionStateRef* state, ValueRef* propertyName, NativeDataAccessorPropertyData* publicData)
{
    ObjectPropertyNativeGetterSetterData* innerData = new ObjectPropertyNativeGetterSetterData(publicData->m_isWritable, publicData->m_isEnumerable, publicData->m_isConfigurable, [](ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea) -> Value {
        NativeDataAccessorPropertyData* publicData = reinterpret_cast<NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
        return toImpl(publicData->m_getter(toRef(&state), toRef(self), publicData));
    },
                                                                                               nullptr);

    if (!publicData->m_isWritable) {
        innerData->m_setter = nullptr;
    } else if (publicData->m_isWritable && !publicData->m_setter) {
        innerData->m_setter = [](ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            return false;
        };
    } else {
        innerData->m_setter = [](ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData) -> bool {
            NativeDataAccessorPropertyData* publicData = reinterpret_cast<NativeDataAccessorPropertyData*>(privateDataFromObjectPrivateArea.payload());
            // ExecutionStateRef* state, ObjectRef* self, NativeDataAccessorPropertyData* data, ValueRef* setterInputData
            return publicData->m_setter(toRef(&state), toRef(self), publicData, toRef(setterInputData));
        };
    }

    return toImpl(this)->defineNativeDataAccessorProperty(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)), innerData, Value(Value::FromPayload, (intptr_t)publicData));
}

bool ObjectRef::set(ExecutionStateRef* state, ValueRef* propertyName, ValueRef* value)
{
    return toImpl(this)->set(*toImpl(state), ObjectPropertyName(*toImpl(state), toImpl(propertyName)), toImpl(value), toImpl(this));
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

bool ObjectRef::isExtensible(ExecutionStateRef* state)
{
    return toImpl(this)->isExtensible(*toImpl(state));
}

bool ObjectRef::preventExtensions(ExecutionStateRef* state)
{
    return toImpl(this)->preventExtensions(*toImpl(state));
}

void* ObjectRef::extraData()
{
    return toImpl(this)->extraData();
}

void ObjectRef::setExtraData(void* e)
{
    toImpl(this)->setExtraData(e);
}

void ObjectRef::removeFromHiddenClassChain()
{
    toImpl(this)->markThisObjectDontNeedStructureTransitionTable();
}

// DEPRECATED
void ObjectRef::removeFromHiddenClassChain(ExecutionStateRef* state)
{
    toImpl(this)->markThisObjectDontNeedStructureTransitionTable();
}

void ObjectRef::enumerateObjectOwnProperies(ExecutionStateRef* state, const std::function<bool(ExecutionStateRef* state, ValueRef* propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>& cb)
{
    toImpl(this)->enumeration(*toImpl(state), [](ExecutionState& state, Object* self, const ObjectPropertyName& name, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
        const std::function<bool(ExecutionStateRef * state, ValueRef * propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>* cb
            = (const std::function<bool(ExecutionStateRef * state, ValueRef * propertyName, bool isWritable, bool isEnumerable, bool isConfigurable)>*)data;
        return (*cb)(toRef(&state), toRef(name.toPlainValue(state)), desc.isWritable(), desc.isEnumerable(), desc.isConfigurable());
    },
                              (void*)&cb);
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

ObjectRef* GlobalObjectRef::promisePrototype()
{
    return toRef(toImpl(this)->promisePrototype());
}

FunctionObjectRef* GlobalObjectRef::arrayBuffer()
{
    return toRef(toImpl(this)->arrayBuffer());
}

FunctionObjectRef* GlobalObjectRef::sharedArrayBuffer()
{
    return toRef(toImpl(this)->sharedArrayBuffer());
}

ObjectRef* GlobalObjectRef::arrayBufferPrototype()
{
    return toRef(toImpl(this)->arrayBufferPrototype());
}

ObjectRef* GlobalObjectRef::sharedArrayBufferPrototype()
{
    return toRef(toImpl(this)->sharedArrayBufferPrototype());
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

class CallPublicFunctionData : public CallNativeFunctionData {
public:
    FunctionObjectRef::NativeFunctionPointer m_publicFn;
};

static Value publicFunctionBridge(ExecutionState& state, Value thisValue, size_t calledArgc, Value* calledArgv, bool isNewExpression)
{
    CodeBlock* dataCb = state.resolveCallee()->codeBlock();
    CallPublicFunctionData* code = (CallPublicFunctionData*)(dataCb->nativeFunctionData());

    ValueRef** newArgv = ALLOCA(sizeof(ValueRef*) * calledArgc, ValueRef*, state);
    for (size_t i = 0; i < calledArgc; i++) {
        newArgv[i] = toRef(calledArgv[i]);
    }
    return toImpl(code->m_publicFn(toRef(&state), toRef(thisValue), calledArgc, newArgv, isNewExpression));
}

static FunctionObjectRef* createFunction(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info, bool isBuiltin)
{
    CallPublicFunctionData* data = new CallPublicFunctionData();
    data->m_fn = publicFunctionBridge;
    data->m_publicFn = info.m_nativeFunction;

    CodeBlock* cb = new CodeBlock(toImpl(state)->context(), toImpl(info.m_name), info.m_argumentCount, info.m_isStrict, info.m_isConstructor, data);
    FunctionObject* f;
    if (isBuiltin)
        f = new NativeFunctionObject(*toImpl(state), cb, NativeFunctionObject::__ForBuiltinConstructor__);
    else
        f = new NativeFunctionObject(*toImpl(state), cb);
    return toRef(f);
}

FunctionObjectRef* FunctionObjectRef::create(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info)
{
    return createFunction(state, info, false);
}

FunctionObjectRef* FunctionObjectRef::createBuiltinFunction(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info)
{
    return createFunction(state, info, true);
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

    InterpretedCodeBlock* child = cb->firstChild();
    while (child) {
        markEvalToCodeblock(child);
        child = child->nextSibling();
    }
}

void FunctionObjectRef::markFunctionNeedsSlowVirtualIdentifierOperation()
{
    FunctionObject* o = toImpl(this);
    if (o->codeBlock()->isInterpretedCodeBlock()) {
        markEvalToCodeblock(o->codeBlock()->asInterpretedCodeBlock());
        o->codeBlock()->setNeedsVirtualIDOperation();
    }
}

GlobalObjectRef* ContextRef::globalObject()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->globalObject());
}

VMInstanceRef* ContextRef::vmInstance()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->vmInstance());
}

void ContextRef::throwException(ValueRef* exceptionValue)
{
    ExecutionState s(toImpl(this));
    toImpl(this)->throwException(s, toImpl(exceptionValue));
}

void ContextRef::setVirtualIdentifierCallback(VirtualIdentifierCallback cb)
{
    Context* ctx = toImpl(this);
    ctx->m_virtualIdentifierCallbackPublic = (void*)cb;
    ctx->setVirtualIdentifierCallback([](ExecutionState& state, Value name) -> Value {
        if (state.context()->m_virtualIdentifierCallbackPublic && !name.isSymbol()) {
            return toImpl(((VirtualIdentifierCallback)state.context()->m_virtualIdentifierCallbackPublic)(toRef(&state), toRef(name)));
        }
        return Value(Value::EmptyValue);
    });
}

ContextRef::VirtualIdentifierCallback ContextRef::virtualIdentifierCallback()
{
    Context* ctx = toImpl(this);
    return ((VirtualIdentifierCallback)ctx->m_virtualIdentifierCallbackPublic);
}

void ContextRef::setSecurityPolicyCheckCallback(SecurityPolicyCheckCallback cb)
{
    Context* ctx = toImpl(this);
    ctx->m_securityPolicyCheckCallbackPublic = (void*)cb;
    ctx->setSecurityPolicyCheckCallback([](ExecutionState& state, bool isEval) -> Value {
        if (state.context()->m_securityPolicyCheckCallbackPublic) {
            return toImpl(((SecurityPolicyCheckCallback)state.context()->m_securityPolicyCheckCallbackPublic)(toRef(&state), isEval));
        }
        return Value(Value::EmptyValue);
    });
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

void ExecutionStateRef::throwException(ValueRef* value)
{
    ExecutionState* imp = toImpl(this);
    imp->throwException(toImpl(value));
}

ValueRef* ValueRef::create(bool value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(int value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(float value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(double value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(long value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned long value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(long long value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(unsigned long long value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::createNull()
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(Value::Null))
                                           .payload());
}

ValueRef* ValueRef::createUndefined()
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(Value::Undefined))
                                           .payload());
}

bool ValueRef::isBoolean()
{
    return toImpl(this).isBoolean();
}

bool ValueRef::isStoreInHeap()
{
    auto value = SmallValue::fromPayload(this);
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

bool ValueRef::toBoolean(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toImpl(this).toBoolean(*esi);
}

double ValueRef::toNumber(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toImpl(this).toNumber(*esi);
}

double ValueRef::toLength(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toImpl(this).toLength(*esi);
}

int32_t ValueRef::toInt32(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toImpl(this).toInt32(*esi);
}

uint32_t ValueRef::toUint32(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toImpl(this).toUint32(*esi);
}

StringRef* ValueRef::toString(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toRef(toImpl(this).toString(*esi));
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
    ExecutionState* esi = toImpl(es);
    return toRef(toImpl(this).toObject(*esi));
}

ValueRef::ValueIndex ValueRef::toIndex(ExecutionStateRef* state)
{
    ExecutionState* esi = toImpl(state);
    return toImpl(this).toIndex(*esi);
}

uint32_t ValueRef::toArrayIndex(ExecutionStateRef* state)
{
    SmallValue s = SmallValue::fromPayload(this);
    if (LIKELY(s.isInt32())) {
        return s.asInt32();
    } else {
        ExecutionState* esi = toImpl(state);
        return Value(s).tryToUseAsArrayIndex(*esi);
    }
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

uint32_t ValueRef::asUint32()
{
    return toImpl(this).asUInt32();
}

PointerValueRef* ValueRef::asPointerValue()
{
    return toRef(toImpl(this).asPointerValue());
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

IteratorObjectRef* IteratorObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new IteratorObject(*toImpl(state)));
}

ValueRef* IteratorObjectRef::next(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->next(*toImpl(state)));
}

ArrayObjectRef* ArrayObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new ArrayObject(*toImpl(state)));
}

ArrayObjectRef* ArrayObjectRef::create(ExecutionStateRef* state, ValueVectorRef* source)
{
    uint64_t sourceSize = source->size();
    ArrayObject* ret = new ArrayObject(*toImpl(state), sourceSize);

    for (size_t i = 0; i < sourceSize; i++) {
        ret->setIndexedProperty(*toImpl(state), Value(i), toImpl(source->at(i)));
    }

    return toRef(ret);
}

IteratorObjectRef* ArrayObjectRef::values(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->values(*toImpl(state)));
}
IteratorObjectRef* ArrayObjectRef::keys(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->keys(*toImpl(state)));
}
IteratorObjectRef* ArrayObjectRef::entries(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->entries(*toImpl(state)));
}

COMPILE_ASSERT((int)ErrorObject::Code::None == (int)ErrorObjectRef::Code::None, "");
COMPILE_ASSERT((int)ErrorObject::Code::ReferenceError == (int)ErrorObjectRef::Code::ReferenceError, "");
COMPILE_ASSERT((int)ErrorObject::Code::TypeError == (int)ErrorObjectRef::Code::TypeError, "");
COMPILE_ASSERT((int)ErrorObject::Code::SyntaxError == (int)ErrorObjectRef::Code::SyntaxError, "");
COMPILE_ASSERT((int)ErrorObject::Code::RangeError == (int)ErrorObjectRef::Code::RangeError, "");
COMPILE_ASSERT((int)ErrorObject::Code::URIError == (int)ErrorObjectRef::Code::URIError, "");
COMPILE_ASSERT((int)ErrorObject::Code::EvalError == (int)ErrorObjectRef::Code::EvalError, "");

ErrorObjectRef* ErrorObjectRef::create(ExecutionStateRef* state, ErrorObjectRef::Code code, StringRef* errorMessage)
{
    return toRef(ErrorObject::createError(*toImpl(state), (ErrorObject::Code)code, toImpl(errorMessage)));
}

ReferenceErrorObjectRef* ReferenceErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new ReferenceErrorObject(*toImpl(state), toImpl(errorMessage)));
}

TypeErrorObjectRef* TypeErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new TypeErrorObject(*toImpl(state), toImpl(errorMessage)));
}

SyntaxErrorObjectRef* SyntaxErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new SyntaxErrorObject(*toImpl(state), toImpl(errorMessage)));
}

RangeErrorObjectRef* RangeErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new RangeErrorObject(*toImpl(state), toImpl(errorMessage)));
}

URIErrorObjectRef* URIErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new URIErrorObject(*toImpl(state), toImpl(errorMessage)));
}

EvalErrorObjectRef* EvalErrorObjectRef::create(ExecutionStateRef* state, StringRef* errorMessage)
{
    return toRef(new EvalErrorObject(*toImpl(state), toImpl(errorMessage)));
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
    return toRef(new SymbolObject(*toImpl(state), new Symbol(String::emptyString)));
}

void SymbolObjectRef::setPrimitiveValue(ExecutionStateRef* state, SymbolRef* value)
{
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(value));
}

SymbolRef* SymbolObjectRef::primitiveValue()
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
    toImpl(this)->setPrimitiveValue(*toImpl(state), toImpl(str).toBoolean(*toImpl(state)));
}

bool BooleanObjectRef::primitiveValue()
{
    return toImpl(this)->primitiveValue();
}

RegExpObjectRef* RegExpObjectRef::create(ExecutionStateRef* state, ValueRef* source, ValueRef* option)
{
    return toRef(new RegExpObject(*toImpl(state), toImpl(source).toString(*toImpl(state)), toImpl(option).toString(*toImpl(state))));
}

StringRef* RegExpObjectRef::source()
{
    return toRef(toImpl(this)->source());
}

RegExpObjectRef::RegExpObjectOption RegExpObjectRef::option()
{
    return (RegExpObjectRef::RegExpObjectOption)toImpl(this)->option();
}

ArrayBufferObjectRef* ArrayBufferObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new ArrayBufferObject(*toImpl(state)));
}

void ArrayBufferObjectRef::allocateBuffer(ExecutionStateRef* state, size_t bytelength)
{
    toImpl(this)->allocateBuffer(*toImpl(state), bytelength);
}

void ArrayBufferObjectRef::attachBuffer(ExecutionStateRef* state, void* buffer, size_t bytelength)
{
    toImpl(this)->attachBuffer(*toImpl(state), buffer, bytelength);
}

void ArrayBufferObjectRef::detachArrayBuffer(ExecutionStateRef* state)
{
    toImpl(this)->detachArrayBuffer(*toImpl(state));
}

uint8_t* ArrayBufferObjectRef::rawBuffer()
{
    return (uint8_t*)toImpl(this)->data();
}

unsigned ArrayBufferObjectRef::byteLength()
{
    return toImpl(this)->byteLength();
}

bool ArrayBufferObjectRef::isDetachedBuffer()
{
    return toImpl(this)->isDetachedBuffer();
}

ArrayBufferObjectRef* ArrayBufferViewRef::buffer()
{
    return toRef(toImpl(this)->buffer());
}

void ArrayBufferViewRef::setBuffer(ArrayBufferObjectRef* bo, unsigned byteOffset, unsigned byteLength, unsigned arrayLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength, arrayLength);
}

void ArrayBufferViewRef::setBuffer(ArrayBufferObjectRef* bo, unsigned byteOffset, unsigned byteLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength);
}
void ArrayBufferViewRef::setBuffer(SharedArrayBufferObjectRef* bo, unsigned byteOffset, unsigned byteLength, unsigned arrayLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength, arrayLength);
}

void ArrayBufferViewRef::setBuffer(SharedArrayBufferObjectRef* bo, unsigned byteOffset, unsigned byteLength)
{
    toImpl(this)->setBuffer(toImpl(bo), byteOffset, byteLength);
}

uint8_t* ArrayBufferViewRef::rawBuffer()
{
    return toImpl(this)->rawBuffer();
}

unsigned ArrayBufferViewRef::byteLength()
{
    return toImpl(this)->byteLength();
}

unsigned ArrayBufferViewRef::byteOffset()
{
    return toImpl(this)->byteOffset();
}

unsigned ArrayBufferViewRef::arrayLength()
{
    return toImpl(this)->arrayLength();
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

Uint8ClampedArrayObjectRef* Uint8ClampedArrayObjectRef::create(ExecutionStateRef* state)
{
    ASSERT(state != nullptr);
    return toRef(new Uint8ClampedArrayObject(*toImpl(state)));
}

PromiseObjectRef* PromiseObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new PromiseObject(*toImpl(state)));
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

PromiseObjectRef* PromiseObjectRef::then(ExecutionStateRef* state, ValueRef* handler)
{
    return toRef(toImpl(this)->then(*toImpl(state), toImpl(handler)));
}

PromiseObjectRef* PromiseObjectRef::catchOperation(ExecutionStateRef* state, ValueRef* handler)
{
    return toRef(toImpl(this)->catchOperation(*toImpl(state), toImpl(handler)));
}

PromiseObjectRef* PromiseObjectRef::then(ExecutionStateRef* state, ValueRef* onFulfilled, ValueRef* onRejected)
{
    return toRef(toImpl(this)->then(*toImpl(state), toImpl(onFulfilled), toImpl(onRejected), toImpl(this)->newPromiseResultCapability(*toImpl(state))).value());
}

void PromiseObjectRef::fulfill(ExecutionStateRef* state, ValueRef* value)
{
    toImpl(this)->fulfill(*toImpl(state), toImpl(value));
}

void PromiseObjectRef::reject(ExecutionStateRef* state, ValueRef* reason)
{
    toImpl(this)->reject(*toImpl(state), toImpl(reason));
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

void WeakSetObjectRef::add(ExecutionStateRef* state, ObjectRef* key)
{
    toImpl(this)->add(*toImpl(state), toImpl(key));
}

bool WeakSetObjectRef::deleteOperation(ExecutionStateRef* state, ObjectRef* key)
{
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key));
}

bool WeakSetObjectRef::has(ExecutionStateRef* state, ObjectRef* key)
{
    return toImpl(this)->has(*toImpl(state), toImpl(key));
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

bool WeakMapObjectRef::deleteOperation(ExecutionStateRef* state, ObjectRef* key)
{
    return toImpl(this)->deleteOperation(*toImpl(state), toImpl(key));
}

ValueRef* WeakMapObjectRef::get(ExecutionStateRef* state, ObjectRef* key)
{
    return toRef(toImpl(this)->get(*toImpl(state), toImpl(key)));
}

void WeakMapObjectRef::set(ExecutionStateRef* state, ObjectRef* key, ValueRef* value)
{
    toImpl(this)->set(*toImpl(state), toImpl(key), toImpl(value));
}

bool WeakMapObjectRef::has(ExecutionStateRef* state, ObjectRef* key)
{
    return toImpl(this)->has(*toImpl(state), toImpl(key));
}

ScriptParserRef::InitializeScriptResult::InitializeScriptResult()
    : script()
    , parseErrorMessage(StringRef::emptyString())
    , parseErrorCode(ErrorObjectRef::Code::None)
{
}

ScriptRef* ScriptParserRef::InitializeScriptResult::fetchScriptThrowsExceptionIfParseError(ExecutionStateRef* state)
{
    if (!script.hasValue()) {
        ErrorObject::throwBuiltinError(*toImpl(state), (ErrorObject::Code)parseErrorCode, toImpl(parseErrorMessage)->toNonGCUTF8StringData().data());
    }

    return script.value();
}

ScriptParserRef::InitializeScriptResult ScriptParserRef::initializeScript(StringRef* script, StringRef* fileName, bool isModule)
{
    auto internalResult = toImpl(this)->initializeScript(toImpl(script), toImpl(fileName), isModule);
    ScriptParserRef::InitializeScriptResult result;
    if (internalResult.script) {
        result.script = toRef(internalResult.script.value());
    } else {
        result.parseErrorMessage = toRef(internalResult.parseErrorMessage);
        result.parseErrorCode = (Escargot::ErrorObjectRef::Code)internalResult.parseErrorCode;
    }

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
    return toRef(toImpl(this)->src());
}

StringRef* ScriptRef::sourceCode()
{
    return toRef(toImpl(this)->sourceCode());
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
}
