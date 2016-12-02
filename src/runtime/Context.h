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

    ObjectStructure* defaultStructureForFunctionObject()
    {
        return m_defaultStructureForFunctionObject;
    }

    ObjectStructure* defaultStructureForNotConstructorFunctionObject()
    {
        return m_defaultStructureForNotConstructorFunctionObject;
    }

    ObjectStructure* defaultStructureForFunctionPrototypeObject()
    {
        return m_defaultStructureForFunctionPrototypeObject;
    }

    GlobalObject* globalObject()
    {
        return m_globalObject;
    }

    // object
    // [__proto__]
    static Value object__proto__NativeGetter(ExecutionState& state, Object* self);
    static bool object__proto__NativeSetter(ExecutionState& state, Object* self, const Value& newData);

    // function
    // [__proto__, name, length] or [__proto__, prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& newData);

protected:
    VMInstance* m_instance;
    StaticStrings m_staticStrings;
    GlobalObject* m_globalObject;
    AtomicStringMap m_atomicStringMap;
    ScriptParser* m_scriptParser;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
};

}

#endif
