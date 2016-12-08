#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "parser/CodeBlock.h"
#include "SandBox.h"
#include "ArrayObject.h"

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
    true, true, true, &Context::object__proto__NativeGetter, &Context::object__proto__NativeSetter);

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

    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);
}

void Context::throwException(ExecutionState& state, const Value& exception)
{
    m_sandBoxStack.back()->throwException(state, exception);
}
}
