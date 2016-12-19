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

namespace Escargot {

Value Context::object__proto__NativeGetter(ExecutionState& state, Object* self)
{
    return self->uncheckedGetOwnDataProperty(state, 0);
}

bool Context::object__proto__NativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    if (newData.isObject() || newData.isUndefinedOrNull()) {
        self->uncheckedSetOwnDataProperty(state, 0, newData);
    }
    return true;
}

static ObjectPropertyNativeGetterSetterData object__proto__NativeGetterSetterData(
    true, false, true, &Context::object__proto__NativeGetter, &Context::object__proto__NativeSetter);

Value Context::functionPrototypeNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isFunctionObject() && self->isPlainObject());
    return self->uncheckedGetOwnDataProperty(state, 1);
}

bool Context::functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    ASSERT(self->isFunctionObject() && self->isPlainObject());
    self->uncheckedSetOwnDataProperty(state, 1, newData);
    return true;
}

static ObjectPropertyNativeGetterSetterData functionPrototypeNativeGetterSetterData(
    true, false, false, &Context::functionPrototypeNativeGetter, &Context::functionPrototypeNativeSetter);

Value Context::arrayLengthNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isArrayObject() && self->isPlainObject());
    return self->uncheckedGetOwnDataProperty(state, 1);
}

bool Context::arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    ASSERT(self->isArrayObject() && self->isPlainObject());
    self->asArrayObject()->setLength(state, newData.toArrayIndex(state));
    return true;
}

static ObjectPropertyNativeGetterSetterData arrayLengthGetterSetterData(
    true, false, false, &Context::arrayLengthNativeGetter, &Context::arrayLengthNativeSetter);

Value Context::stringLengthNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isStringObject());
    return Value(self->asStringObject()->primitiveValue()->length());
}

bool Context::stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    return false;
}

static ObjectPropertyNativeGetterSetterData stringLengthGetterSetterData(
    false, false, false, &Context::stringLengthNativeGetter, &Context::stringLengthNativeSetter);

Value Context::regexpSourceNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    return Value(self->asRegExpObject()->source());
}
static ObjectPropertyNativeGetterSetterData regexpSourceGetterData(
    false, false, false, &Context::regexpSourceNativeGetter, nullptr);

Value Context::regexpGlobalNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::Global));
}

static ObjectPropertyNativeGetterSetterData regexpGlobalGetterData(
    false, false, false, &Context::regexpGlobalNativeGetter, nullptr);

Value Context::regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::IgnoreCase));
}

static ObjectPropertyNativeGetterSetterData regexpIgnoreCaseGetterData(
    false, false, false, &Context::regexpIgnoreCaseNativeGetter, nullptr);

Value Context::regexpMultilineNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    return Value((bool)(self->asRegExpObject()->option() & RegExpObject::Option::MultiLine));
}

static ObjectPropertyNativeGetterSetterData regexpMultilineGetterData(
    false, false, false, &Context::regexpMultilineNativeGetter, nullptr);

Value Context::regexpLastIndexNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    return self->asRegExpObject()->lastIndex();
}

bool Context::regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    ASSERT(self->isRegExpObject() && self->isPlainObject());
    self->asRegExpObject()->setLastIndex(newData);
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

    ObjectStructure defaultStructureForObject(stateForInit);
    m_defaultStructureForObject = defaultStructureForObject.addProperty(stateForInit, m_staticStrings.__proto__,
                                                                        ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&object__proto__NativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                   ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&functionPrototypeNativeGetterSetterData));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                           ObjectPropertyDescriptor::createDataDescriptor(ObjectPropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionObject = m_defaultStructureForFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                           ObjectPropertyDescriptor::createDataDescriptor(ObjectPropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.name,
                                                                                                 ObjectPropertyDescriptor::createDataDescriptor(ObjectPropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForNotConstructorFunctionObject = m_defaultStructureForNotConstructorFunctionObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                                                       ObjectPropertyDescriptor::createDataDescriptor(ObjectPropertyDescriptor::ConfigurablePresent));

    m_defaultStructureForFunctionPrototypeObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.prototype,
                                                                                            ObjectPropertyDescriptor::createDataDescriptor((ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    m_defaultStructureForArrayObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length,
                                                                                ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&arrayLengthGetterSetterData));

    m_defaultStructureForStringObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.length, ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&stringLengthGetterSetterData));

    m_defaultStructureForRegExpObject = m_defaultStructureForObject->addProperty(stateForInit, m_staticStrings.lastIndex,
                                                                                 ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpLastIndexGetterSetterData));
    // TODO(ES6): Below RegExp data properties is changed to accessor properties of RegExp.prototype in ES6.
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.source,
                                                                                       ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpSourceGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.global,
                                                                                       ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpGlobalGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.ignoreCase,
                                                                                       ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpIgnoreCaseGetterData));
    m_defaultStructureForRegExpObject = m_defaultStructureForRegExpObject->addProperty(stateForInit, m_staticStrings.multiline,
                                                                                       ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&regexpMultilineGetterData));
    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);

    // TODO call destructor
    m_bumpPointerAllocator = new (GC) WTF::BumpPointerAllocator();
}

void Context::throwException(ExecutionState& state, const Value& exception)
{
    m_sandBoxStack.back()->throwException(state, exception);
}
}
