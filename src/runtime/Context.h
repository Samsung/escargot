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
class JobQueue;
class ByteCodeBlock;

class Context : public gc {
    friend class AtomicString;
    friend class SandBox;
    friend class ByteCodeInterpreter;

public:
    Context(VMInstance* instance);

    VMInstance* vmInstance()
    {
        return m_instance;
    }

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

    ObjectStructure* defaultStructureForBuiltinFunctionObject()
    {
        return m_defaultStructureForBuiltinFunctionObject;
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

    ObjectStructure* defaultStructureForArgumentsObject()
    {
        return m_defaultStructureForArgumentsObject;
    }

    ObjectStructure* defaultStructureForArgumentsObjectInStrictMode()
    {
        return m_defaultStructureForArgumentsObjectInStrictMode;
    }

    GlobalObject* globalObject()
    {
        return m_globalObject;
    }

    // object
    // []

    // function
    // [name, length] or [prototype, name, length]
    static Value functionPrototypeNativeGetter(ExecutionState& state, Object* self);
    static bool functionPrototypeNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // array
    // [length]
    static Value arrayLengthNativeGetter(ExecutionState& state, Object* self);
    static bool arrayLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // string
    // [length]
    static Value stringLengthNativeGetter(ExecutionState& state, Object* self);
    static bool stringLengthNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);

    // regexp
    // [lastIndex, source, global, ignoreCase, multiline]
    static bool regexpLastIndexNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData);
    static Value regexpLastIndexNativeGetter(ExecutionState& state, Object* self);
    static Value regexpSourceNativeGetter(ExecutionState& state, Object* self);
    static Value regexpGlobalNativeGetter(ExecutionState& state, Object* self);
    static Value regexpIgnoreCaseNativeGetter(ExecutionState& state, Object* self);
    static Value regexpMultilineNativeGetter(ExecutionState& state, Object* self);

    bool didSomePrototypeObjectDefineIndexedProperty()
    {
        return m_didSomePrototypeObjectDefineIndexedProperty;
    }

    void somePrototypeObjectDefineIndexedProperty(ExecutionState& state);
    void throwException(ExecutionState& state, const Value& exception);

#if ESCARGOT_ENABLE_PROMISE
    JobQueue* jobQueue()
    {
        return m_jobQueue;
    }
#endif

    Vector<CodeBlock*, gc_allocator<CodeBlock*>>& compiledCodeBlocks()
    {
        return m_compiledCodeBlocks;
    }

protected:
    bool m_didSomePrototypeObjectDefineIndexedProperty;
    VMInstance* m_instance;
    StaticStrings m_staticStrings;
    GlobalObject* m_globalObject;
    AtomicStringMap m_atomicStringMap;
    ScriptParser* m_scriptParser;
    Vector<CodeBlock*, gc_allocator<CodeBlock*>> m_compiledCodeBlocks;

    // To support Yarr
    WTF::BumpPointerAllocator* m_bumpPointerAllocator;
    RegExpCacheMap m_regexpCache;

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForArrayObject;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForArgumentsObject;
    ObjectStructure* m_defaultStructureForArgumentsObjectInStrictMode;

    Vector<SandBox*, gc_malloc_allocator<SandBox*>> m_sandBoxStack;

#if ESCARGOT_ENABLE_PROMISE
    JobQueue* m_jobQueue;
#endif
};
}

#endif
