/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include <cstdlib> // size_t

#include "Escargot.h"
#include "GCUtil.h"
#include "util/Vector.h"
#include "EscargotPublic.h"
#include "parser/ScriptParser.h"
#include "runtime/Context.h"
#include "runtime/ExecutionContext.h"
#include "runtime/FunctionObject.h"
#include "runtime/Value.h"
#include "runtime/VMInstance.h"
#include "runtime/SandBox.h"
#include "runtime/Environment.h"
#include "runtime/Job.h"
#include "runtime/JobQueue.h"

namespace Escargot {

/////////// TODO (implement platform layer)
class DefaultJobQueue : public JobQueue {
private:
    DefaultJobQueue() {}
public:
    static DefaultJobQueue* create()
    {
        return new DefaultJobQueue();
    }

    size_t enqueueJob(Job* job)
    {
        m_jobs.push_back(job);
        return 0;
    }

    bool hasNextJob()
    {
        return !m_jobs.empty();
    }

    Job* nextJob()
    {
        ASSERT(!m_jobs.empty());
        Job* job = m_jobs.front();
        m_jobs.pop_front();
        return job;
    }

    static DefaultJobQueue* get(JobQueue* jobQueue)
    {
        return (DefaultJobQueue*)jobQueue;
    }

private:
    std::list<Job*, gc_allocator<Job*> > m_jobs;
};

#ifndef ESCARGOT_SHELL
JobQueue* JobQueue::create()
{
    return DefaultJobQueue::create();
}
#endif
///////////

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
DEFINE_CAST(PointerValue);
DEFINE_CAST(Object);
DEFINE_CAST(FunctionObject);
DEFINE_CAST(Script);
DEFINE_CAST(ScriptParser);

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

std::string StringRef::toStdUTF8String()
{
    auto ret = toImpl(this)->toUTF8StringData();
    return std::string(ret.data(), ret.length());
}

VMInstanceRef* VMInstanceRef::create()
{
    return toRef(new VMInstance());
}

void VMInstanceRef::destroy()
{
    VMInstance* imp = toImpl(this);
    delete imp;
}

ContextRef* ContextRef::create(VMInstanceRef* vminstanceref)
{
    VMInstance* vminstance = toImpl(vminstanceref);
    return toRef(new Context(vminstance));
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


// can not redefine or delete virtual object
class ExposableObject : public Object {
public:
    ExposableObject(ExecutionState& state, bool isWritable, bool isEnumerable, bool isConfigurable, ExposableObjectGetOwnPropertyCallback getOwnPropetyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback, ExposableObjectEnumerationCallback enumerationCallback)
        : Object(state)
        , m_isWritable(isWritable)
        , m_isEnumerable(isEnumerable)
        , m_isConfigurable(isConfigurable)
        , m_getOwnPropetyCallback(getOwnPropetyCallback)
        , m_defineOwnPropertyCallback(defineOwnPropertyCallback)
        , m_enumerationCallback(enumerationCallback)
    {
    }

    virtual ObjectGetResult getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value result = toImpl(m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toValue(state))));
        if (!result.isEmpty()) {
            return ObjectGetResult(result, m_isWritable, m_isEnumerable, m_isConfigurable);
        }
        return Object::getOwnProperty(state, P);
    }
    virtual bool defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value result = toImpl(m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toValue(state))));
        if (!result.isEmpty()) {
            if (desc.isValuePresent()) {
                m_defineOwnPropertyCallback(toRef(&state), toRef(this), toRef(P.toValue(state)), toRef(desc.value()));
            }
            return false;
        }
        return Object::defineOwnProperty(state, P, desc);
    }
    virtual bool deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        Value result = toImpl(m_getOwnPropetyCallback(toRef(&state), toRef(this), toRef(P.toValue(state))));
        if (!result.isEmpty()) {
            return false;
        }
        return Object::deleteOwnProperty(state, P);
    }
    virtual void enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
    {
        ValueVectorRef* names = m_enumerationCallback(toRef(&state), toRef(this));
        int attr = 0;
        if (m_isWritable) {
            attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::WritablePresent;
        }
        if (m_isEnumerable) {
            attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::EnumerablePresent;
        }
        if (m_isConfigurable) {
            attr = attr | ObjectStructurePropertyDescriptor::PresentAttribute::ConfigurablePresent;
        }
        ObjectStructurePropertyDescriptor desc = ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)attr);
        for (size_t i = 0; i < names->size(); i++) {
            callback(state, this, ObjectPropertyName(state, toImpl(names->at(i))), desc, data);
        }
        Object::enumeration(state, callback, data);
    }

    virtual bool isInlineCacheable()
    {
        return false;
    }

protected:
    bool m_isWritable : 1;
    bool m_isEnumerable : 1;
    bool m_isConfigurable : 1;
    ExposableObjectGetOwnPropertyCallback m_getOwnPropetyCallback;
    ExposableObjectDefineOwnPropertyCallback m_defineOwnPropertyCallback;
    ExposableObjectEnumerationCallback m_enumerationCallback;
};

ObjectRef* ObjectRef::createExposableObject(ExecutionStateRef* state,
                                            ExposableObjectGetOwnPropertyCallback getOwnPropertyCallback, ExposableObjectDefineOwnPropertyCallback defineOwnPropertyCallback,
                                            ExposableObjectEnumerationCallback enumerationCallback, bool isWritable, bool isEnumerable, bool isConfigurable)
{
    return toRef(new ExposableObject(*toImpl(state), isWritable, isEnumerable, isConfigurable, getOwnPropertyCallback, defineOwnPropertyCallback, enumerationCallback));
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

COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NotPresent == (int)ObjectPropertyDescriptor::NotPresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::WritablePresent == (int)ObjectPropertyDescriptor::WritablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::EnumerablePresent == (int)ObjectPropertyDescriptor::EnumerablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::ConfigurablePresent == (int)ObjectPropertyDescriptor::ConfigurablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonWritablePresent == (int)ObjectPropertyDescriptor::NonWritablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonEnumerablePresent == (int)ObjectPropertyDescriptor::NonEnumerablePresent, "");
COMPILE_ASSERT((int)ObjectRef::PresentAttribute::NonConfigurablePresent == (int)ObjectPropertyDescriptor::NonConfigurablePresent, "");

bool ObjectRef::defineDataPropety(ExecutionStateRef* state, ValueRef* propertyName, const DataPropertyDescriptor& desc)
{
    return toImpl(this)->defineOwnProperty(*toImpl(state),
                                           ObjectPropertyName(*toImpl(state), toImpl(propertyName)), ObjectPropertyDescriptor(toImpl(desc.m_value), (ObjectPropertyDescriptor::PresentAttribute)desc.m_attribute));
}

bool ObjectRef::defineAccessorPropety(ExecutionStateRef* state, ValueRef* propertyName, const AccessorPropertyDescriptor& desc)
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

bool ObjectRef::isExtensible()
{
    return toImpl(this)->isExtensible();
}

void ObjectRef::preventExtensions()
{
    toImpl(this)->preventExtensions();
}

// http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
const char* ObjectRef::internalClassProperty()
{
    return toImpl(this)->internalClassProperty();
}

void ObjectRef::giveInternalClassProperty(const char* name)
{
    toImpl(this)->giveInternalClassProperty(name);
}

void* ObjectRef::extraData()
{
    return toImpl(this)->extraData();
}

void ObjectRef::setExtraData(void* e)
{
    toImpl(this)->setExtraData(e);
}


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

FunctionObjectRef* FunctionObjectRef::create(ExecutionStateRef* state, FunctionObjectRef::NativeFunctionInfo info)
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
    FunctionObject* f = new FunctionObject(*toImpl(state), cb, nullptr);
    return toRef(f);
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
    , loc(SIZE_MAX, SIZE_MAX, SIZE_MAX)
{
}

SandBoxRef::SandBoxResult::SandBoxResult()
    : result(ValueRef::createEmpty())
    , error(ValueRef::createEmpty())
    , msgStr(toRef(String::emptyString))
{
}

SandBoxRef::SandBoxResult SandBoxRef::run(const std::function<ValueRef*(ExecutionStateRef* state)>& scriptRunner)
{
    auto result = toImpl(this)->run([&]() -> Value {
        ExecutionState state(toImpl(this)->context());
        return toImpl(scriptRunner(toRef(&state)));
    });

    SandBoxRef::SandBoxResult r;
    r.error = toRef(result.error);
    r.msgStr = toRef(result.msgStr);
    r.result = toRef(result.result);

    if (!result.error.isEmpty()) {
        for (size_t i = 0; i < result.stackTraceData.size(); i++) {
            SandBoxRef::StackTraceData t;
            t.fileName = toRef(result.stackTraceData[i].fileName);
            t.loc.line = result.stackTraceData[i].loc.line;
            t.loc.column = result.stackTraceData[i].loc.column;
            r.stackTraceData.pushBack(t);
        }
    }

    return r;
}

ObjectRef* ContextRef::globalObject()
{
    Context* ctx = toImpl(this);
    return toRef(ctx->globalObject());
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


ValueRef* ValueRef::create(bool value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(int32_t value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(uint32_t value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(value)).payload());
}

ValueRef* ValueRef::create(double value)
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

ValueRef* ValueRef::create(PointerValueRef* value)
{
    return reinterpret_cast<ValueRef*>(SmallValue(Value(toImpl(value)))
                                           .payload());
}

bool ValueRef::isBoolean() const
{
    return Value(SmallValue::fromPayload(this)).isBoolean();
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
