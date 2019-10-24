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
#include "ArrayObject.h"
#include "StringObject.h"
#include "JobQueue.h"
#include "parser/ASTAllocator.h"

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
    return privateDataFromObjectPrivateArea;
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

Value VMInstance::arrayLengthNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    ASSERT(self->isArrayObject());
    return privateDataFromObjectPrivateArea;
}

bool VMInstance::arrayLengthNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isArrayObject());

    bool isPrimitiveValue;
    if (LIKELY(setterInputData.isPrimitive())) {
        isPrimitiveValue = true;
    } else {
        isPrimitiveValue = false;
    }
    // Let newLen be ToUint32(Desc.[[Value]]).
    auto newLen = setterInputData.toUint32(state);
    // If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
    if (newLen != setterInputData.toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
    }

    bool ret;
    if (UNLIKELY(!isPrimitiveValue && !self->structure()->readProperty(state, (size_t)0).m_descriptor.isWritable())) {
        ret = false;
    } else {
        ret = self->asArrayObject()->setArrayLength(state, newLen);
    }
    return ret;
}

static ObjectPropertyNativeGetterSetterData arrayLengthGetterSetterData(
    true, false, false, &VMInstance::arrayLengthNativeGetter, &VMInstance::arrayLengthNativeSetter);

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

VMInstance::~VMInstance()
{
    if (m_onVMInstanceDestroy) {
        m_onVMInstanceDestroy(this, m_onVMInstanceDestroyData);
    }
    clearCaches();
#ifdef ENABLE_ICU
    delete m_timezone;
#endif
    delete m_astAllocator;
}

VMInstance::VMInstance(Platform* platform, const char* locale, const char* timezone)
    : m_currentSandBox(nullptr)
    , m_randEngine((unsigned int)time(NULL))
    , m_didSomePrototypeObjectDefineIndexedProperty(false)
    , m_compiledByteCodeSize(0)
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

    if (!String::emptyString) {
        String::emptyString = new (NoGC) ASCIIString("");
    }
    m_staticStrings.initStaticStrings(&m_atomicStringMap);

    // TODO call destructor
    m_bumpPointerAllocator = new (GC) WTF::BumpPointerAllocator();

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
        m_locale = icu::Locale::createFromName(locale);
    } else if (getenv("LOCALE")) {
        m_locale = icu::Locale::createFromName(getenv("LOCALE"));
    } else {
        m_locale = icu::Locale::getDefault();
    }
#endif

    g_doubleInSmallValueTag = DoubleInSmallValue(0).getTag();

    g_objectRareDataTag = ObjectRareData(nullptr).getTag();

    g_symbolTag = Symbol(nullptr).getTag();

#define DECLARE_GLOBAL_SYMBOLS(name) m_globalSymbols.name = new Symbol(String::fromASCII("Symbol." #name));
    DEFINE_GLOBAL_SYMBOLS(DECLARE_GLOBAL_SYMBOLS);
#undef DECLARE_GLOBAL_SYMBOLS

    ExecutionState stateForInit((Context*)nullptr);

    m_defaultStructureForObject = new ObjectStructure(stateForInit);

    m_defaultStructureForFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                   ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&functionPrototypeNativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                          ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                 ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForNotConstructorFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                                       ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBoundFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBoundFunctionObject = m_defaultStructureForBoundFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionPrototypeObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.constructor,
                                                                                            ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForClassConstructorFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                   ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForClassConstructorFunctionObject = m_defaultStructureForClassConstructorFunctionObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                                                           ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));


    m_defaultStructureForArrayObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&arrayLengthGetterSetterData));

    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForSymbolObject = m_defaultStructureForObject;

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.lastIndex,
                                                                                 ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));

    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(stateForInit, PropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForMappedArgumentsObject = m_defaultStructureForMappedArgumentsObject->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(stateForInit, PropertyName(stateForInit, globalSymbols().iterator), ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(stateForInit, m_staticStrings.caller, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));
    m_defaultStructureForUnmappedArgumentsObject = m_defaultStructureForUnmappedArgumentsObject->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));

    m_jobQueue = new JobQueue();
}

void VMInstance::clearCaches()
{
    m_compiledCodeBlocks.clear();
    m_regexpCache.clear();
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
    std::vector<ArrayObject*, GCUtil::gc_malloc_allocator<ArrayObject*>> allOfArrayRooted;
    allOfArrayRooted.assign(allOfArray.begin(), allOfArray.end());
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
