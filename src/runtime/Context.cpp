#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"

namespace Escargot {

static ObjectPropertyNativeGetterSetterData object__proto__NativeGetterSetterData;

Value Context::object__proto__NativeGetter(ExecutionState& state, Object* self)
{
    return self->uncheckedGetOwnProperty(0);
}

bool Context::object__proto__NativeSetter(ExecutionState& state, Object* self, const Value& newData)
{
    if (newData.isObject() || newData.isUndefinedOrNull()) {
        self->uncheckedSetOwnProperty(0, newData);
    }
    return true;
}


Context::Context(VMInstance* instance)
    : m_instance(instance)
    , m_scriptParser(new ScriptParser(this))
{
    m_staticStrings.initStaticStrings(&m_atomicStringMap);
    ExecutionState stateForInit(this);
    m_globalObject = new GlobalObject(stateForInit);

    object__proto__NativeGetterSetterData.m_isWritable = true;
    object__proto__NativeGetterSetterData.m_isEnumerable = true;
    object__proto__NativeGetterSetterData.m_isConfigurable = true;
    object__proto__NativeGetterSetterData.m_getter = object__proto__NativeGetter;
    object__proto__NativeGetterSetterData.m_setter = object__proto__NativeSetter;

    ObjectStructure defaultStructureForObject(stateForInit);
    m_defaultStructureForObject = defaultStructureForObject.addProperty(stateForInit, m_staticStrings.__proto__.string(),
        ObjectPropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(&object__proto__NativeGetterSetterData));
}

}
