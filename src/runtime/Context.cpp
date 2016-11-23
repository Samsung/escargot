#include "Escargot.h"
#include "Context.h"
#include "GlobalObject.h"
#include "parser/ScriptParser.h"

namespace Escargot {

Context::Context(VMInstance* instance)
    : m_instance(instance)
    , m_scriptParser(new ScriptParser(this))
{
    m_staticStrings.initStaticStrings(&m_atomicStringMap);
    ExecutionState stateForInit(this);
    m_globalObject = new GlobalObject(stateForInit);
}

}
