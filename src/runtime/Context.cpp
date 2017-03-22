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

#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
#include "StringObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "parser/CodeBlock.h"
#include "SandBox.h"
#include "ArrayObject.h"
#include "BumpPointerAllocator.h"
#include "JobQueue.h"

namespace Escargot {

Value Context::functionPrototypeNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isFunctionObject());
    return self->m_values[0];
}

bool Context::functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData)
{
    ASSERT(self->isFunctionObject());
    self->m_values[0] = setterInputData;
    return true;
}

static ObjectPropertyNativeGetterSetterData functionPrototypeNativeGetterSetterData(
    true, false, false, &Context::functionPrototypeNativeGetter, &Context::functionPrototypeNativeSetter);

static ObjectPropertyNativeGetterSetterData builtinFunctionPrototypeNativeGetterSetterData(
    false, false, false, &Context::functionPrototypeNativeGetter, &Context::functionPrototypeNativeSetter);

Value Context::arrayLengthNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isArrayObject());
    return self->m_values[0];
}

bool Context::arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData)
{
    ASSERT(self->isArrayObject());

    // Let newLen be ToUint32(Desc.[[Value]]).
    auto newLen = setterInputData.toUint32(state);
    // If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
    if (newLen != setterInputData.toNumber(state)) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::RangeError, errorMessage_GlobalObject_InvalidArrayLength);
    }

    bool ret = self->asArrayObject()->setArrayLength(state, newLen);
    return ret;
}

static ObjectPropertyNativeGetterSetterData arrayLengthGetterSetterData(
    true, false, false, &Context::arrayLengthNativeGetter, &Context::arrayLengthNativeSetter);

Value Context::stringLengthNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isStringObject());
    return Value(self->asStringObject()->primitiveValue()->length());
}

bool Context::stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData)
{
    return false;
}

static ObjectPropertyNativeGetterSetterData stringLengthGetterSetterData(
    false, false, false, &Context::stringLengthNativeGetter, &Context::stringLengthNativeSetter);

Value Context::regexpSourceNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject());
    return Value(self->asRegExpObject()->source());
}
static ObjectPropertyNativeGetterSetterData regexpSourceGetterData(
    false, false, false, &Context::regexpSourceNativeGetter, nullptr);

Value Context::regexpGlobalNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::Global));
}

static ObjectPropertyNativeGetterSetterData regexpGlobalGetterData(
    false, false, false, &Context::regexpGlobalNativeGetter, nullptr);

Value Context::regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::IgnoreCase));
}

static ObjectPropertyNativeGetterSetterData regexpIgnoreCaseGetterData(
    false, false, false, &Context::regexpIgnoreCaseNativeGetter, nullptr);

Value Context::regexpMultilineNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::MultiLine));
}

static ObjectPropertyNativeGetterSetterData regexpMultilineGetterData(
    false, false, false, &Context::regexpMultilineNativeGetter, nullptr);

Value Context::regexpLastIndexNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject());
    return self->asRegExpObject()->lastIndex();
}

bool Context::regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData)
{
    ASSERT(self->isRegExpObject());
    self->asRegExpObject()->setLastIndex(state, setterInputData);
    return true;
}

static ObjectPropertyNativeGetterSetterData regexpLastIndexGetterSetterData(
    true, false, false, &Context::regexpLastIndexNativeGetter, &Context::regexpLastIndexNativeSetter);

Context::Context(VMInstance* instance)
    : m_didSomePrototypeObjectDefineIndexedProperty(false)
    , m_instance(instance)
    , m_scriptParser(new ScriptParser(this))
{
    m_staticStrings.initStaticStrings(&m_atomicStringMap);
    ExecutionState stateForInit(this);

#if ESCARGOT_ENABLE_PROMISE
    m_jobQueue = JobQueue::create();
#endif

    ObjectStructure defaultStructureForObject(stateForInit);
    m_defaultStructureForObject = new ObjectStructure(stateForInit);

    m_defaultStructureForFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                   ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&functionPrototypeNativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                           ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::ConfigurablePresent));

    // TODO(ES6)
    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.length,
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

    m_defaultStructureForFunctionPrototypeObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.constructor,
                                                                                            ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForArrayObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&arrayLengthGetterSetterData));

    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.lastIndex,
                                                                                 ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));
    // TODO(ES6): Below RegExp data properties is changed to accessor properties of RegExp.prototype in ES6.
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.source,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpSourceGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.global,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpGlobalGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.ignoreCase,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpIgnoreCaseGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.multiline,
                                                                                       ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpMultilineGetterData));

    m_defaultStructureForArgumentsObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObject = m_defaultStructureForArgumentsObject->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObject = m_defaultStructureForArgumentsObject->addProperty(stateForInit, m_staticStrings.caller, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));

    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::ConfigurablePresent)));
    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForArgumentsObjectInStrictMode->addProperty(stateForInit, m_staticStrings.callee, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));
    m_defaultStructureForArgumentsObjectInStrictMode = m_defaultStructureForArgumentsObjectInStrictMode->addProperty(stateForInit, m_staticStrings.caller, ObjectStructurePropertyDescriptor::createAccessorDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::NotPresent)));

    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);

    // TODO call destructor
    m_bumpPointerAllocator = new (GC) WTF::BumpPointerAllocator();

    auto temp = new ArrayObject(stateForInit);
    g_arrayObjectTag = *((size_t*)temp);
}

void Context::throwException(ExecutionState& state, const Value& exception)
{
    m_sandBoxStack.back()->throwException(state, exception);
}

void Context::somePrototypeObjectDefineIndexedProperty(ExecutionState& state)
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
}
