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
#include "runtime/ExecutionContext.h"
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
#ifdef ESCARGOT_ENABLE_PROMISE
#include "runtime/Job.h"
#include "runtime/JobQueue.h"
#include "runtime/PromiseObject.h"
#endif
#ifdef ESCARGOT_ENABLE_TYPEDARRAY
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayObject.h"
#endif

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
DEFINE_CAST(SandBox);
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
#ifdef ESCARGOT_ENABLE_PROMISE
DEFINE_CAST(PromiseObject);
DEFINE_CAST(Job);
#endif
DEFINE_CAST(Script);
DEFINE_CAST(ScriptParser);

#if ESCARGOT_ENABLE_TYPEDARRAY
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
#endif

#undef DEFINE_CAST


inline ValueRef* toRef(const Value& v)
{
    return reinterpret_cast<ValueRef*>(SmallValue(v).payload());
}

inline Value toImpl(ValueRef* v)
{
    return Value(SmallValue::fromPayload(v));
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

void Globals::initialize(bool applyMallOpt, bool applyGcOpt)
{
    Heap::initialize(applyMallOpt, applyGcOpt);
}

void Globals::finalize()
{
    return;
}

StringRef* StringRef::fromASCII(const char* s)
{
    return toRef(new ASCIIString(s, strlen(s)));
}

StringRef* StringRef::fromASCII(const char* s, size_t len)
{
    return toRef(new ASCIIString(s, len));
}

StringRef* StringRef::fromUTF8(const char* s, size_t len)
{
    return toRef(String::fromUTF8(s, len));
}

StringRef* StringRef::fromUTF16(const char16_t* s, size_t len)
{
    return toRef(new UTF16String(s, len));
}

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

StringRef* SymbolRef::description()
{
    return toRef(toImpl(this)->description());
}

bool PointerValueRef::isString()
{
    return toImpl(this)->isString();
}

StringRef* PointerValueRef::asString()
{
    return toRef(toImpl(this)->asString());
}

bool PointerValueRef::isSymbol()
{
    return toImpl(this)->isSymbol();
}

SymbolRef* PointerValueRef::asSymbol()
{
    return toRef(toImpl(this)->asSymbol());
}

bool PointerValueRef::isObject()
{
    return toImpl(this)->isString();
}

ObjectRef* PointerValueRef::asObject()
{
    return toRef(toImpl(this)->asObject());
}

bool PointerValueRef::isFunctionObject()
{
    return toImpl(this)->isFunctionObject();
}

FunctionObjectRef* PointerValueRef::asFunctionObject()
{
    return toRef(toImpl(this)->asFunctionObject());
}

bool PointerValueRef::isArrayObject()
{
    return toImpl(this)->isArrayObject();
}

ArrayObjectRef* PointerValueRef::asArrayObject()
{
    return toRef(toImpl(this)->asArrayObject());
}

bool PointerValueRef::isStringObject()
{
    return toImpl(this)->isStringObject();
}

StringObjectRef* PointerValueRef::asStringObject()
{
    return toRef(toImpl(this)->asStringObject());
}

bool PointerValueRef::isSymbolObject()
{
    return toImpl(this)->isSymbolObject();
}

SymbolObjectRef* PointerValueRef::asSymbolObject()
{
    return toRef(toImpl(this)->asSymbolObject());
}

bool PointerValueRef::isNumberObject()
{
    return toImpl(this)->isNumberObject();
}

NumberObjectRef* PointerValueRef::asNumberObject()
{
    return toRef(toImpl(this)->asNumberObject());
}

bool PointerValueRef::isBooleanObject()
{
    return toImpl(this)->isBooleanObject();
}

BooleanObjectRef* PointerValueRef::asBooleanObject()
{
    return toRef(toImpl(this)->asBooleanObject());
}

bool PointerValueRef::isRegExpObject()
{
    return toImpl(this)->isRegExpObject();
}

RegExpObjectRef* PointerValueRef::asRegExpObject()
{
    return toRef(toImpl(this)->asRegExpObject());
}

bool PointerValueRef::isDateObject()
{
    return toImpl(this)->isDateObject();
}

DateObjectRef* PointerValueRef::asDateObject()
{
    return toRef(toImpl(this)->asDateObject());
}

bool PointerValueRef::isGlobalObject()
{
    return toImpl(this)->isGlobalObject();
}

GlobalObjectRef* PointerValueRef::asGlobalObject()
{
    return toRef(toImpl(this)->asGlobalObject());
}

bool PointerValueRef::isErrorObject()
{
    return toImpl(this)->isErrorObject();
}

ErrorObjectRef* PointerValueRef::asErrorObject()
{
    return toRef(toImpl(this)->asErrorObject());
}

#if ESCARGOT_ENABLE_TYPEDARRAY
bool PointerValueRef::isArrayBufferObject()
{
    return toImpl(this)->isArrayBufferObject();
}

ArrayBufferObjectRef* PointerValueRef::asArrayBufferObject()
{
    return toRef(toImpl(this)->asArrayBufferObject());
}

bool PointerValueRef::isArrayBufferView()
{
    return toImpl(this)->isArrayBufferView();
}

ArrayBufferViewRef* PointerValueRef::asArrayBufferView()
{
    return toRef(toImpl(this)->asArrayBufferView());
}

#define DEFINE_TYPEDARRAY_IMPL(TypeName)                                                                                             \
    bool PointerValueRef::is##TypeName##ArrayObject()                                                                                \
    {                                                                                                                                \
        return toImpl(this)->isArrayBufferView() && toImpl(this)->asArrayBufferView()->typedArrayType() == TypedArrayType::TypeName; \
    }                                                                                                                                \
                                                                                                                                     \
    TypeName##ArrayObjectRef* PointerValueRef::as##TypeName##ArrayObject()                                                           \
    {                                                                                                                                \
        ASSERT(is##TypeName##ArrayObject());                                                                                         \
        return toRef((TypeName##ArrayObject*)(toImpl(this)->asArrayBufferView()));                                                   \
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

#endif
#if ESCARGOT_ENABLE_PROMISE
bool PointerValueRef::isPromiseObject()
{
    return toImpl(this)->isPromiseObject();
}
PromiseObjectRef* PointerValueRef::asPromiseObject()
{
    return toRef(toImpl(this)->asPromiseObject());
}
#endif

VMInstanceRef* VMInstanceRef::create(const char* locale, const char* timezone)
{
    return toRef(new (NoGC) VMInstance(locale, timezone));
}

void VMInstanceRef::destroy()
{
    VMInstance* imp = toImpl(this);
    delete imp;
}

void VMInstanceRef::clearCachesRelatedWithContext()
{
    VMInstance* imp = toImpl(this);
    imp->m_compiledCodeBlocks.clear();
    imp->m_regexpCache.clear();
    imp->m_cachedUTC = nullptr;
    imp->globalSymbolRegistry().clear();
}

bool VMInstanceRef::addRoot(VMInstanceRef* instanceRef, ValueRef* ptr)
{
    auto value = SmallValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return false;
    }
    void* vptr = reinterpret_cast<void*>(value.payload());
    toImpl(this)->addRoot(vptr);
    return true;
}

bool VMInstanceRef::removeRoot(VMInstanceRef* instanceRef, ValueRef* ptr)
{
    auto value = SmallValue::fromPayload(ptr);
    if (!value.isStoredInHeap()) {
        return false;
    }
    void* vptr = reinterpret_cast<void*>(value.payload());
    return toImpl(this)->removeRoot(vptr);
}

SymbolRef* VMInstanceRef::toStringTagSymbol()
{
    return toRef(toImpl(this)->globalSymbols().toStringTag);
}

SymbolRef* VMInstanceRef::iteratorSymbol()
{
    return toRef(toImpl(this)->globalSymbols().iterator);
}

SymbolRef* VMInstanceRef::unscopablesSymbol()
{
    return toRef(toImpl(this)->globalSymbols().unscopables);
}

#ifdef ESCARGOT_ENABLE_PROMISE
ValueRef* VMInstanceRef::drainJobQueue()
{
    VMInstance* imp = toImpl(this);
    return toRef(imp->drainJobQueue());
}

void VMInstanceRef::setNewPromiseJobListener(NewPromiseJobListener l)
{
    VMInstance* imp = toImpl(this);
    imp->m_publicJobQueueListenerPointer = (void*)l;
    imp->setNewPromiseJobListener([](ExecutionState& state, Job* job) {
        ((NewPromiseJobListener)state.context()->vmInstance()->m_publicJobQueueListenerPointer)(toRef(&state), toRef(job));
    });
}

#endif

ContextRef* ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = toImpl(vminstanceref);
    return toRef(new Context(vminstance));
}

void ContextRef::clearRelatedQueuedPromiseJobs()
{
    Context* imp = toImpl(this);
#ifdef ESCARGOT_ENABLE_PROMISE
    DefaultJobQueue* jobQueue = DefaultJobQueue::get(imp->vmInstance()->jobQueue());
    std::list<Job*, gc_allocator<Job*>>& impl = jobQueue->impl();
    auto iter = impl.begin();
    while (iter != impl.end()) {
        if ((*iter)->relatedContext() == imp) {
            iter = impl.erase(iter);
        } else {
            iter++;
        }
    }
#endif
}

void ContextRef::destroy()
{
    Context* imp = toImpl(this);
    delete imp;
}

ContextRef* ExecutionStateRef::context()
{
    return toRef(toImpl(this)->context());
}

AtomicStringRef* AtomicStringRef::create(ContextRef* c, const char* src)
{
    AtomicString a(toImpl(c), src, strlen(src));
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
    return toRef(new (GC) SmallValueVector(size));
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

ValueRef* ValueVectorRef::at(const size_t& idx)
{
    return reinterpret_cast<ValueRef*>((*toImpl(this))[idx].payload());
}

void ValueVectorRef::set(const size_t& idx, ValueRef* newValue)
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
            if (!result.m_value->isEmpty()) {
                return ObjectGetResult(toImpl(result.m_value), result.m_isWritable, result.m_isEnumerable, result.m_isConfigurable);
            }
        }
        return Object::getOwnProperty(state, P);
    }
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        // Only value type supported
        if (desc.isValuePresent()) {
            Value PV = P.toPlainValue(state);
            if (!PV.isSymbol()) {
                if (m_defineOwnPropertyCallback(toRef(&state), toRef(this), toRef(PV), toRef(desc.value()))) {
                    return true;
                }
            }
        }
        return Object::defineOwnProperty(state, P, desc);
    }
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value PV = P.toPlainValue(state);
        if (!PV.isSymbol()) {
            auto result = m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toPlainValue(state)));
            if (!result.m_value->isEmpty()) {
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

protected:
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

ValueRef* ObjectRef::getPrototype(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->getPrototype(*toImpl(state)));
}

ObjectRef* ObjectRef::getPrototypeObject()
{
    return toRef(toImpl(this)->getPrototypeObject());
}

void ObjectRef::setPrototype(ExecutionStateRef* state, ValueRef* value)
{
    toImpl(this)->setPrototype(*toImpl(state), toImpl(value));
}

ValueVectorRef* ObjectRef::getOwnPropertyKeys(ExecutionStateRef* state)
{
    ValueVector v = toImpl(this)->getOwnPropertyKeys(*toImpl(state));

    ValueVectorRef* result = ValueVectorRef::create(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        result->set(i, toRef(v[i]));
    }

    return result;
}

bool ObjectRef::isExtensible()
{
    return toImpl(this)->isExtensible();
}

void ObjectRef::preventExtensions()
{
    toImpl(this)->preventExtensions();
}

void* ObjectRef::extraData()
{
    return toImpl(this)->extraData();
}

void ObjectRef::setExtraData(void* e)
{
    toImpl(this)->setExtraData(e);
}

void ObjectRef::removeFromHiddenClassChain(ExecutionStateRef* state)
{
    toImpl(this)->markThisObjectDontNeedStructureTransitionTable(*toImpl(state));
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


#if ESCARGOT_ENABLE_PROMISE
FunctionObjectRef* GlobalObjectRef::promise()
{
    return toRef(toImpl(this)->promise());
}

ObjectRef* GlobalObjectRef::promisePrototype()
{
    return toRef(toImpl(this)->promisePrototype());
}

#endif

#if ESCARGOT_ENABLE_TYPEDARRAY
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

#endif

class CallPublicFunctionData : public CallNativeFunctionData {
public:
    FunctionObjectRef::NativeFunctionPointer m_publicFn;
    FunctionObjectRef::NativeFunctionConstructor m_publicCtor;
};

static Value publicFunctionBridge(ExecutionState& state, Value thisValue, size_t calledArgc, Value* calledArgv, bool isNewExpression)
{
    CodeBlock* dataCb = state.executionContext()->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock();
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
    if (info.m_isConstructor && info.m_nativeFunctionConstructor) {
        data->m_publicCtor = info.m_nativeFunctionConstructor;
        data->m_ctorFn = [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) -> Object* {
            ValueRef** newArgv = ALLOCA(sizeof(ValueRef*) * argc, ValueRef*, state);
            for (size_t i = 0; i < argc; i++) {
                newArgv[i] = toRef(argv[i]);
            }
            return toImpl(((CallPublicFunctionData*)codeBlock->nativeFunctionData())->m_publicCtor(toRef(&state), argc, newArgv));
        };
    } else if (info.m_isConstructor) {
        data->m_publicCtor = nullptr;
        data->m_ctorFn = [](ExecutionState& state, CodeBlock* cb, size_t argc, Value* argv) -> Object* {
            return new Object(state);
        };
    } else {
        data->m_publicCtor = nullptr;
        data->m_ctorFn = nullptr;
    }

    CodeBlock* cb = new CodeBlock(toImpl(state)->context(), toImpl(info.m_name), info.m_argumentCount, info.m_isStrict, info.m_isConstructor, data);
    FunctionObject* f;
    if (isBuiltin)
        f = new FunctionObject(*toImpl(state), cb, FunctionObject::__ForBuiltin__);
    else
        f = new FunctionObject(*toImpl(state), cb, nullptr);
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

ValueRef* FunctionObjectRef::call(ExecutionStateRef* state, ValueRef* receiver, const size_t& argc, ValueRef** argv)
{
    FunctionObject* o = toImpl(this);
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value, state);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(o->call(*toImpl(state), toImpl(receiver), argc, newArgv));
}

ObjectRef* FunctionObjectRef::newInstance(ExecutionStateRef* state, const size_t& argc, ValueRef** argv)
{
    FunctionObject* o = toImpl(this);
    Value* newArgv = ALLOCA(sizeof(Value) * argc, Value, state);
    for (size_t i = 0; i < argc; i++) {
        newArgv[i] = toImpl(argv[i]);
    }
    return toRef(o->newInstance(*toImpl(state), argc, newArgv));
}

static void markEvalToCodeblock(InterpretedCodeBlock* cb)
{
    cb->setHasEval();
    cb->computeVariables();

    for (size_t i = 0; i < cb->childBlocks().size(); i++) {
        markEvalToCodeblock(cb->childBlocks()[i]->asInterpretedCodeBlock());
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

SandBoxRef* SandBoxRef::create(ContextRef* contextRef)
{
    Context* ctx = toImpl(contextRef);
    return toRef(new SandBox(ctx));
}

void SandBoxRef::destroy()
{
    SandBox* imp = toImpl(this);
    delete imp;
}

SandBoxRef::StackTraceData::StackTraceData()
    : fileName(toRef(String::emptyString))
    , source(toRef(String::emptyString))
    , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
{
}

SandBoxRef::SandBoxResult::SandBoxResult()
    : result(ValueRef::createEmpty())
    , error(ValueRef::createEmpty())
    , msgStr(toRef(String::emptyString))
{
}

static SandBoxRef::SandBoxResult toSandBoxResultRef(SandBox::SandBoxResult& result)
{
    SandBoxRef::SandBoxResult r;
    r.error = toRef(result.error);
    r.msgStr = toRef(result.msgStr);
    r.result = toRef(result.result);

    if (!result.error.isEmpty()) {
        for (size_t i = 0; i < result.stackTraceData.size(); i++) {
            SandBoxRef::StackTraceData t;
            t.fileName = toRef(result.stackTraceData[i].fileName);
            t.source = toRef(result.stackTraceData[i].source);
            t.loc.index = result.stackTraceData[i].loc.index;
            t.loc.line = result.stackTraceData[i].loc.line;
            t.loc.column = result.stackTraceData[i].loc.column;
            r.stackTraceData.push_back(t);
        }
    }

    return r;
}

SandBoxRef::SandBoxResult SandBoxRef::run(const std::function<ValueRef*(ExecutionStateRef* state)>& scriptRunner)
{
    auto result = toImpl(this)->run([&]() -> Value {
        ExecutionState state(toImpl(this)->context());
        return toImpl(scriptRunner(toRef(&state)));
    });
    return toSandBoxResultRef(result);
}

SandBoxRef::SandBoxResult JobRef::run()
{
    auto result = toImpl(this)->run();
    return toSandBoxResultRef(result);
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

void ContextRef::setVirtualIdentifierCallback(VirtualIdentifierCallback cb)
{
    Context* ctx = toImpl(this);
    ctx->m_virtualIdentifierCallbackPublic = (void*)cb;
    ctx->setVirtualIdentifierCallback([](ExecutionState& state, Value name) -> Value {
        if (state.context()->m_virtualIdentifierCallbackPublic) {
            if (!name.isSymbol()) {
                return toImpl(((VirtualIdentifierCallback)state.context()->m_virtualIdentifierCallbackPublic)(toRef(&state), toRef(name)));
            }
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

ExecutionStateRef* ExecutionStateRef::create(ContextRef* ctxref)
{
    Context* ctx = toImpl(ctxref);
    return toRef(new ExecutionState(ctx));
}

void ExecutionStateRef::destroy()
{
    ExecutionState* imp = toImpl(this);
    delete imp;
}

FunctionObjectRef* ExecutionStateRef::resolveCallee()
{
    auto ec = toImpl(this)->executionContext();
    if (ec) {
        return toRef(ec->resolveCallee());
    }
    return nullptr;
}

std::vector<std::pair<FunctionObjectRef*, ValueRef*>> ExecutionStateRef::resolveCallstack()
{
    ExecutionState* state = toImpl(this);

    std::vector<std::pair<FunctionObjectRef*, ValueRef*>> result;

    while (state) {
        if (state->executionContext() && state->executionContext()->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord()
            && state->executionContext()->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            auto r = state->executionContext()->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
            FunctionObject* callee = r->functionObject();
            Value thisValue = state->registerFile()[0];
            result.push_back(std::make_pair(toRef(callee), toRef(thisValue)));
        }
        state = state->parent();
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

ValueRef* ValueRef::createEmpty()
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(Value::EmptyValue))
                                           .payload());
}

ValueRef* ValueRef::createUndefined()
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(Value::Undefined))
                                           .payload());
}

bool ValueRef::isBoolean() const
{
    return Value(SmallValue::fromPayload(this)).isBoolean();
}

bool ValueRef::isStoreInHeap() const
{
    auto value = SmallValue::fromPayload(this);
    if (value.isStoredInHeap()) {
        return true;
    }
    return false;
}

bool ValueRef::isNumber() const
{
    return Value(SmallValue::fromPayload(this)).isNumber();
}

bool ValueRef::isNull() const
{
    return Value(SmallValue::fromPayload(this)).isNull();
}

bool ValueRef::isUndefined() const
{
    return Value(SmallValue::fromPayload(this)).isUndefined();
}

bool ValueRef::isString() const
{
    return Value(SmallValue::fromPayload(this)).isString();
}

bool ValueRef::isObject() const
{
    return Value(SmallValue::fromPayload(this)).isObject();
}

bool ValueRef::isInt32() const
{
    return Value(SmallValue::fromPayload(this)).isInt32();
}

bool ValueRef::isUInt32() const
{
    return Value(SmallValue::fromPayload(this)).isUInt32();
}

bool ValueRef::isDouble() const
{
    return Value(SmallValue::fromPayload(this)).isDouble();
}

bool ValueRef::isTrue() const
{
    return Value(SmallValue::fromPayload(this)).isTrue();
}

bool ValueRef::isFalse() const
{
    return Value(SmallValue::fromPayload(this)).isFalse();
}

bool ValueRef::isEmpty() const
{
    return Value(SmallValue::fromPayload(this)).isEmpty();
}

bool ValueRef::isFunction() const
{
    return Value(SmallValue::fromPayload(this)).isFunction();
}

bool ValueRef::toBoolean(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(this)).toBoolean(*esi);
}

double ValueRef::toNumber(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(this)).toNumber(*esi);
}

double ValueRef::toLength(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(this)).toLength(*esi);
}

int32_t ValueRef::toInt32(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(this)).toInt32(*esi);
}

uint32_t ValueRef::toUint32(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return Value(SmallValue::fromPayload(this)).toUint32(*esi);
}

StringRef* ValueRef::toString(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toRef(Value(SmallValue::fromPayload(this)).toString(*esi));
}

ObjectRef* ValueRef::toObject(ExecutionStateRef* es)
{
    ExecutionState* esi = toImpl(es);
    return toRef(Value(SmallValue::fromPayload(this)).toObject(*esi));
}

ValueRef::ValueIndex ValueRef::toIndex(ExecutionStateRef* state)
{
    ExecutionState* esi = toImpl(state);
    return Value(SmallValue::fromPayload(this)).toIndex(*esi);
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
    return Value(SmallValue::fromPayload(this)).asBoolean();
}

double ValueRef::asNumber()
{
    return Value(SmallValue::fromPayload(this)).asNumber();
}

int32_t ValueRef::asInt32()
{
    return Value(SmallValue::fromPayload(this)).asInt32();
}

uint32_t ValueRef::asUint32()
{
    return Value(SmallValue::fromPayload(this)).asUInt32();
}

StringRef* ValueRef::asString()
{
    return toRef(Value(SmallValue::fromPayload(this)).asString());
}

ObjectRef* ValueRef::asObject()
{
    return toRef(Value(SmallValue::fromPayload(this)).asObject());
}

FunctionObjectRef* ValueRef::asFunction()
{
    return toRef(Value(SmallValue::fromPayload(this)).asFunction());
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

#ifdef ESCARGOT_ENABLE_TYPEDARRAY

void ArrayBufferObjectRef::setMallocFunction(ArrayBufferObjectBufferMallocFunction fn)
{
    g_arrayBufferObjectBufferMallocFunction = fn;
}

void ArrayBufferObjectRef::setFreeFunction(ArrayBufferObjectBufferFreeFunction fn)
{
    g_arrayBufferObjectBufferFreeFunction = fn;
}

void ArrayBufferObjectRef::setMallocFunctionNeedsZeroFill(bool n)
{
    g_arrayBufferObjectBufferMallocFunctionNeedsZeroFill = n;
}

ArrayBufferObjectRef* ArrayBufferObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new ArrayBufferObject(*toImpl(state)));
}

void ArrayBufferObjectRef::allocateBuffer(size_t bytelength)
{
    toImpl(this)->allocateBuffer(bytelength);
}

void ArrayBufferObjectRef::attachBuffer(void* buffer, size_t bytelength)
{
    toImpl(this)->attachBuffer(buffer, bytelength);
}

void ArrayBufferObjectRef::detachArrayBuffer()
{
    toImpl(this)->detachArrayBuffer();
}

uint8_t* ArrayBufferObjectRef::rawBuffer()
{
    return (uint8_t*)toImpl(this)->data();
}

unsigned ArrayBufferObjectRef::bytelength()
{
    return toImpl(this)->bytelength();
}

ArrayBufferObjectRef* ArrayBufferViewRef::buffer()
{
    return toRef(toImpl(this)->buffer());
}

uint8_t* ArrayBufferViewRef::rawBuffer()
{
    return toImpl(this)->rawBuffer();
}

unsigned ArrayBufferViewRef::bytelength()
{
    return toImpl(this)->bytelength();
}

Uint8ClampedArrayObjectRef* Uint8ClampedArrayObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new Uint8ClampedArrayObject(*toImpl(state)));
}
#endif

#ifdef ESCARGOT_ENABLE_PROMISE
PromiseObjectRef* PromiseObjectRef::create(ExecutionStateRef* state)
{
    return toRef(new PromiseObject(*toImpl(state)));
}
void PromiseObjectRef::fulfill(ExecutionStateRef* state, ValueRef* value)
{
    toImpl(this)->fulfillPromise(*toImpl(state), toImpl(value));
}

void PromiseObjectRef::reject(ExecutionStateRef* state, ValueRef* reason)
{
    toImpl(this)->rejectPromise(*toImpl(state), toImpl(reason));
}
#endif

ScriptParserRef::ScriptParserResult ScriptParserRef::parse(StringRef* script, StringRef* fileName)
{
    auto result = toImpl(this)->parse(toImpl(script), toImpl(fileName));
    if (result.m_error) {
        return ScriptParserRef::ScriptParserResult(nullptr, toRef(result.m_error->message));
    }
    return ScriptParserRef::ScriptParserResult(toRef(result.m_script), StringRef::emptyString());
}

ValueRef* ScriptRef::execute(ExecutionStateRef* state)
{
    return toRef(toImpl(this)->execute(*toImpl(state)));
}
}
