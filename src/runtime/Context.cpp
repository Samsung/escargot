#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
namespace Escargot {

Context::Context(VMInstance* instance)
    : m_instance(instance)
{
    m_staticStrings.initStaticStrings(&m_atomicStringMap);
    ExecutionState stateForInit(this);
    m_globalObject = new GlobalObject(stateForInit);
}

}
