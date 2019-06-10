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

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_objectRareDataTag;
extern size_t g_symbolTag;

#ifndef ESCARGOT_ENABLE_ES2015
bool VMInstance::undefinedNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    return false;
}
#endif

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

#ifndef ESCARGOT_ENABLE_ES2015
Value VMInstance::regexpSourceNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }

    return Value(self->asRegExpObject(state)->source());
}

static ObjectPropertyNativeGetterSetterData regexpSourceGetterData(
    false, false, false, &VMInstance::regexpSourceNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpFlagsNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value(self->asRegExpObject(state)->optionString(state));
}

static ObjectPropertyNativeGetterSetterData regexpFlagsGetterData(
    false, false, true, &VMInstance::regexpFlagsNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpGlobalNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value((bool)(self->asRegExpObject(state)->option() & RegExpObject::Option::Global));
}

static ObjectPropertyNativeGetterSetterData regexpGlobalGetterData(
    false, false, false, &VMInstance::regexpGlobalNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value((bool)(self->asRegExpObject(state)->option() & RegExpObject::Option::IgnoreCase));
}

static ObjectPropertyNativeGetterSetterData regexpIgnoreCaseGetterData(
    false, false, false, &VMInstance::regexpIgnoreCaseNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpMultilineNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value((bool)(self->asRegExpObject(state)->option() & RegExpObject::Option::MultiLine));
}

static ObjectPropertyNativeGetterSetterData regexpMultilineGetterData(
    false, false, false, &VMInstance::regexpMultilineNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpStickyNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value((bool)(self->asRegExpObject(state)->option() & RegExpObject::Option::Sticky));
}

static ObjectPropertyNativeGetterSetterData regexpStickyGetterData(
    false, false, false, &VMInstance::regexpStickyNativeGetter, &VMInstance::undefinedNativeSetter);

Value VMInstance::regexpUnicodeNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return Value((bool)(self->asRegExpObject(state)->option() & RegExpObject::Option::Unicode));
}

static ObjectPropertyNativeGetterSetterData regexpUnicodeGetterData(
    false, false, false, &VMInstance::regexpUnicodeNativeGetter, &VMInstance::undefinedNativeSetter);
#endif

Value VMInstance::regexpLastIndexNativeGetter(ExecutionState& state, Object* self, const SmallValue& privateDataFromObjectPrivateArea)
{
    if (self->isRegExpObject(state) == false) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, "getter called on non-RegExp object");
    }
    return self->asRegExpObject(state)->lastIndex();
}

bool VMInstance::regexpLastIndexNativeSetter(ExecutionState& state, Object* self, SmallValue& privateDataFromObjectPrivateArea, const Value& setterInputData)
{
    ASSERT(self->isRegExpObject(state) == true);
    self->asRegExpObject(state)->setLastIndex(state, setterInputData);
    return true;
}

static ObjectPropertyNativeGetterSetterData regexpLastIndexGetterSetterData(
    true, false, false, &VMInstance::regexpLastIndexNativeGetter, &VMInstance::regexpLastIndexNativeSetter);

VMInstance::VMInstance(const char* locale, const char* timezone)
    : m_randEngine((unsigned int)time(NULL))
    , m_didSomePrototypeObjectDefineIndexedProperty(false)
    , m_compiledByteCodeSize(0)
    , m_cachedUTC(nullptr)
{
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
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    // TODO(ES6)

    // Class Function
    m_defaultStructureForClassFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));
    m_defaultStructureForClassFunctionObject = m_defaultStructureForClassFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForClassFunctionObject = m_defaultStructureForClassFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    m_defaultStructureForClassFunctionObject = m_defaultStructureForClassFunctionObject->addProperty(stateForInit, m_staticStrings.caller,
                                                                                                     ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

    m_defaultStructureForClassFunctionObject = m_defaultStructureForClassFunctionObject->addProperty(stateForInit, m_staticStrings.arguments,
                                                                                                     ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

    //Arrow Function

    m_defaultStructureForArrowFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForArrowFunctionObject = m_defaultStructureForArrowFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    m_defaultStructureForFunctionObjectInStrictMode = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.caller,
                                                                                                       ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

    m_defaultStructureForFunctionObjectInStrictMode = m_defaultStructureForFunctionObjectInStrictMode->addProperty(stateForInit, m_staticStrings.arguments,
                                                                                                                   ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                          ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&builtinFunctionPrototypeNativeGetterSetterData));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForBuiltinFunctionObject = m_defaultStructureForBuiltinFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                         ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                 ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForNotConstructorFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                                       ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    m_defaultStructureForNotConstructorFunctionObjectInStrictMode = m_defaultStructureForNotConstructorFunctionObject->addProperty(stateForInit, m_staticStrings.caller,
                                                                                                                                   ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

    m_defaultStructureForNotConstructorFunctionObjectInStrictMode = m_defaultStructureForNotConstructorFunctionObjectInStrictMode->addProperty(stateForInit, m_staticStrings.arguments,
                                                                                                                                               ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::WritablePresent));

#ifndef ESCARGOT_ENABLE_ES2015
    m_defaultStructureForBoundFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::NotPresent));
#else
    m_defaultStructureForBoundFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                        ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));
#endif

    m_defaultStructureForBoundFunctionObject = m_defaultStructureForBoundFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                     ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

#ifndef ESCARGOT_ENABLE_ES2015
    m_defaultStructureForBoundFunctionObject = m_defaultStructureForBoundFunctionObject->addProperty(stateForInit, m_staticStrings.caller,
                                                                                                     ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::NotPresent));

    m_defaultStructureForBoundFunctionObject = m_defaultStructureForBoundFunctionObject->addProperty(stateForInit, m_staticStrings.arguments,
                                                                                                     ObjectStructurePropertyDescriptor::createAccessorDescriptor(ObjectStructurePropertyDescriptor::NotPresent));
#endif

    m_defaultStructureForFunctionPrototypeObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.constructor,
                                                                                            ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForArrayObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&arrayLengthGetterSetterData));

    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForSymbolObject = m_defaultStructureForObject;

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.lastIndex,
                                                                                 ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));
#ifndef ESCARGOT_ENABLE_ES2015
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.source,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpSourceGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.flags,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpFlagsGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.global,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpGlobalGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.ignoreCase,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpIgnoreCaseGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.multiline,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpMultilineGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.sticky,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpStickyGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.unicode,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpUnicodeGetterData));
#endif

    m_defaultStructureForArgumentsObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObject = m_defaultStructureForArgumentsObject->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObject = m_defaultStructureForArgumentsObject->addProperty(stateForInit, m_staticStrings.caller, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForArgumentsObjectInStrictMode->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));
    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForArgumentsObjectInStrictMode->addProperty(stateForInit, m_staticStrings.caller, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));

#if ESCARGOT_ENABLE_PROMISE
    m_jobQueue = JobQueue::create();
    m_jobQueueListener = nullptr;
    m_publicJobQueueListenerPointer = nullptr;
#endif
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
    std::vector<ArrayObject*, GCUtil::gc_malloc_ignore_off_page_allocator<ArrayObject*>> allOfArrayRooted;
    allOfArrayRooted.assign(allOfArray.begin(), allOfArray.end());
    GC_enable();

    for (size_t i = 0; i < allOfArrayRooted.size(); i++) {
        allOfArrayRooted[i]->convertIntoNonFastMode(state);
    }
}

void VMInstance::addRoot(void* ptr)
{
    auto iter = m_rootSet.find(ptr);
    if (iter == m_rootSet.end()) {
        m_rootSet.insert(std::make_pair(ptr, 1));
    } else {
        iter->second++;
    }
}

bool VMInstance::removeRoot(void* ptr)
{
    auto iter = m_rootSet.find(ptr);
    if (iter == m_rootSet.end()) {
        return false;
    } else {
        iter->second++;
        if (iter->second == 0) {
            m_rootSet.erase(iter);
        }
    }
    return true;
}

Value VMInstance::drainJobQueue()
{
    ASSERT(!m_jobQueueListener);

    DefaultJobQueue* jobQueue = DefaultJobQueue::get(this->jobQueue());
    while (jobQueue->hasNextJob()) {
        auto jobResult = jobQueue->nextJob()->run();
        if (!jobResult.error.isEmpty())
            return jobResult.error;
    }
    return Value(Value::EmptyValue);
}

void VMInstance::setNewPromiseJobListener(NewPromiseJobListener l)
{
    m_jobQueueListener = l;
}
}
