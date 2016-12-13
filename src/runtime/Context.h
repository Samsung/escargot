#ifndef __EscargotContext__
#define __EscargotContext__

#include "runtime/AtomicString.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "runtime/RegExpObject.h"
#include "runtime/StaticStrings.h"
#include "runtime/String.h"

namespace WTF {
class BumpPointerAllocator;
}

namespace Escargot {

class VMInstance;
class ScriptParser;
class ObjectStructure;
class ControlFlowRecord;
class SandBox;

class Context : public gc {
    friend class AtomicString;
    friend class SandBox;
    friend class ByteCodeInterpreter;

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

    RegExpCacheMap* regexpCache()
    {
        return &m_regexpCache;
    }

    WTF::BumpPointerAllocator* bumpPointerAllocator()
    {
        return m_bumpPointerAllocator;
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

    ObjectStructure* defaultStructureForArrayObject()
    {
        return m_defaultStructureForArrayObject;
    }

    ObjectStructure* defaultStructureForStringObject()
    {
        return m_defaultStructureForStringObject;
    }

    ObjectStructure* defaultStructureForRegExpObject()
    {
        return m_defaultStructureForRegExpObject;
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

    // array
    // [__proto__, length]
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& newData);

    // regexp
    // [__proto__, lastIndex, source, global, ignoreCase, multiline
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& newData);
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self);
    static Value regexpSourceNativeGetter(ExecutionState& state, Object* self);
    static Value regexpGlobalNativeGetter(ExecutionState& state, Object* self);
    static Value regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self);
    static Value regexpMultilineNativeGetter(ExecutionState& state, Object* self);

    bool didSomePrototypeObjectDefineIndexedProperty()
    {
        return m_didSomePrototypeObjectDefineIndexedProperty;
    }

    void throwException(ExecutionState& state, const Value& exception);

protected:
    bool m_didSomePrototypeObjectDefineIndexedProperty;
    VMInstance* m_instance;
    StaticStrings m_staticStrings;
    GlobalObject* m_globalObject;
    AtomicStringMap m_atomicStringMap;
    ScriptParser* m_scriptParser;

    // To support Yarr
    WTF::BumpPointerAllocator* m_bumpPointerAllocator;
    RegExpCacheMap m_regexpCache;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForArrayObject;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForRegExpObject;

    Vector<SandBox*, gc_malloc_allocator<SandBox*>> m_sandBoxStack;
};
}

#endif
