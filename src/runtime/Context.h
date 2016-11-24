#ifndef __EscargotContext__
#define __EscargotContext__

#include "runtime/Context.h"
#include "runtime/String.h"
#include "runtime/AtomicString.h"
#include "runtime/StaticStrings.h"
#include "runtime/GlobalObject.h"

namespace Escargot {

class VMInstance;
class ScriptParser;
class ObjectStructure;

class Context : public gc {
    friend class AtomicString;
public:
    Context(VMInstance* instance);
    const StaticStrings& staticStrings()
    {
        return m_staticStrings;
    }

    ScriptParser& scriptParser()
    {
        return *m_scriptParser;
    }

    ObjectStructure* defaultStructureForObject()
    {
        return m_defaultStructureForObject;
    }
protected:
    VMInstance* m_instance;
    StaticStrings m_staticStrings;
    GlobalObject* m_globalObject;
    AtomicStringMap m_atomicStringMap;
    ScriptParser* m_scriptParser;

    ObjectStructure* m_defaultStructureForObject;

    static Value object__proto__NativeGetter(ExecutionState& state, Object* self);
    static bool object__proto__NativeSetter(ExecutionState& state, Object* self, const Value& newData);
};

}

#endif
