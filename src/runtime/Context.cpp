#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "parser/CodeBlock.h"

namespace Escargot {

Value Context::object__proto__NativeGetter(ExecutionState& state, Object* self)
{
    return self->uncheckedGetOwnPlainDataProperty(state, 0);
}

bool Context::object__proto__NativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    if (newData.isObject() || newData.isUndefinedOrNull()) {
        self->uncheckedSetOwnPlainDataProperty(state, 0, newData);
    }
    return true;
}

static ObjectPropertyNativeGetterSetterData object__proto__NativeGetterSetterData(
        true, true, true, &Context::object__proto__NativeGetter, &Context::object__proto__NativeSetter);

Value Context::functionPrototypeNativeGetter(ExecutionState& state, Object* self)
{
    ASSERT(self->isFunctionObject() && self->isPlainObject());
    return self->uncheckedGetOwnPlainDataProperty(state, 1);
}

bool Context::functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    ASSERT(self->isFunctionObject() && self->isPlainObject());
    self->uncheckedSetOwnPlainDataProperty(state, 1, newData);
    return true;
}

static ObjectPropertyNativeGetterSetterData functionPrototypeNativeGetterSetterData(
        true, false, false, &Context::functionPrototypeNativeGetter, &Context::functionPrototypeNativeSetter);

Context::Context(VMInstance* instance)
    : m_instance(instance)
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

    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);
}

}
