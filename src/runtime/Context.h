/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __EscargotContext__
#define __EscargotContext__

#include "runtime/AtomicString.h"
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
class ByteCodeBlock;
class ToStringRecursionPreventer;

struct IdentifierRecord {
    AtomicString m_name;
    bool m_canDelete : 1;
    bool m_isMutable : 1;
    bool m_isVarDeclaration : 1;
};

typedef Vector<IdentifierRecord, GCUtil::gc_malloc_atomic_allocator<IdentifierRecord>> IdentifierRecordVector;

typedef Value (*VirtualIdentifierCallback)(ExecutionState& state, Value name);
typedef Value (*SecurityPolicyCheckCallback)(ExecutionState& state, bool isEval);

struct GlobalVariableAccessCacheItem : public gc {
    size_t m_lexicalIndexCache;
    AtomicString m_propertyName;
    void* m_cachedAddress;
    ObjectStructure* m_cachedStructure;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class Context : public gc {
    friend class AtomicString;
    friend class SandBox;
    friend class ByteCodeInterpreter;
    friend struct OpcodeTable;
    friend class ContextRef;

public:
    explicit Context(VMInstance* instance);

    VMInstance* vmInstance()
    {
        return m_instance;
    }

    const StaticStrings& staticStrings()
    {
        return m_staticStrings;
    }

    AtomicStringMap* atomicStringMap()
    {
        return m_atomicStringMap;
    }

    ScriptParser& scriptParser()
    {
        return *m_scriptParser;
    }

    RegExpCacheMap* regexpCache()
    {
        return m_regexpCache;
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

    ObjectStructure* defaultStructureForBuiltinFunctionObject()
    {
        return m_defaultStructureForBuiltinFunctionObject;
    }

    ObjectStructure* defaultStructureForBoundFunctionObject()
    {
        return m_defaultStructureForBoundFunctionObject;
    }

    ObjectStructure* defaultStructureForFunctionPrototypeObject()
    {
        return m_defaultStructureForFunctionPrototypeObject;
    }

    ObjectStructure* defaultStructureForClassConstructorFunctionObject()
    {
        return m_defaultStructureForClassConstructorFunctionObject;
    }

    ObjectStructure* defaultStructureForArrayObject()
    {
        return m_defaultStructureForArrayObject;
    }

    ObjectStructure* defaultStructureForStringObject()
    {
        return m_defaultStructureForStringObject;
    }

    ObjectStructure* defaultStructureForSymbolObject()
    {
        return m_defaultStructureForSymbolObject;
    }

    ObjectStructure* defaultStructureForRegExpObject()
    {
        return m_defaultStructureForRegExpObject;
    }

    ObjectStructure* defaultStructureForMappedArgumentsObject()
    {
        return m_defaultStructureForMappedArgumentsObject;
    }

    ObjectStructure* defaultStructureForUnmappedArgumentsObject()
    {
        return m_defaultStructureForUnmappedArgumentsObject;
    }

    GlobalObject* globalObject()
    {
        return m_globalObject;
    }

    ToStringRecursionPreventer* toStringRecursionPreventer()
    {
        return m_toStringRecursionPreventer;
    }

    void throwException(ExecutionState& state, const Value& exception);

    Vector<CodeBlock*, GCUtil::gc_malloc_allocator<CodeBlock*>>& compiledCodeBlocks()
    {
        return m_compiledCodeBlocks;
    }

    // this is not compatible with ECMAScript
    // but this callback is needed for browser-implementation
    // if there is a Identifier with that value, callback should return non-empty value
    void setVirtualIdentifierCallback(VirtualIdentifierCallback cb)
    {
        m_virtualIdentifierCallback = cb;
    }

    VirtualIdentifierCallback virtualIdentifierCallback()
    {
        return m_virtualIdentifierCallback;
    }

    void setSecurityPolicyCheckCallback(SecurityPolicyCheckCallback cb)
    {
        m_securityPolicyCheckCallback = cb;
    }

    SecurityPolicyCheckCallback securityPolicyCheckCallback()
    {
        return m_securityPolicyCheckCallback;
    }

    IdentifierRecordVector& globalDeclarativeRecord()
    {
        return m_globalDeclarativeRecord;
    }

    SmallValueVector& globalDeclarativeStorage()
    {
        return m_globalDeclarativeStorage;
    }

    GlobalVariableAccessCacheItem* ensureGlobalVariableAccessCacheSlot(AtomicString as);

private:
    VMInstance* m_instance;

    // these data actually store in VMInstance
    // we can store pointers of these data for reducing memory dereference to VMInstance
    AtomicStringMap* m_atomicStringMap;
    StaticStrings& m_staticStrings;
    GlobalObject* m_globalObject;
    ScriptParser* m_scriptParser;
    IdentifierRecordVector m_globalDeclarativeRecord;
    SmallValueVector m_globalDeclarativeStorage;
    std::unordered_map<AtomicString, GlobalVariableAccessCacheItem*, std::hash<AtomicString>, std::equal_to<AtomicString>,
                       GCUtil::gc_malloc_allocator<std::pair<AtomicString, GlobalVariableAccessCacheItem*>>>
        m_globalVariableAccessCache;
    Vector<CodeBlock*, GCUtil::gc_malloc_allocator<CodeBlock*>>& m_compiledCodeBlocks;
    WTF::BumpPointerAllocator* m_bumpPointerAllocator;
    RegExpCacheMap* m_regexpCache;
    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForBoundFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForArrayObject;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForSymbolObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForMappedArgumentsObject;
    ObjectStructure* m_defaultStructureForUnmappedArgumentsObject;
    Vector<SandBox*, GCUtil::gc_malloc_allocator<SandBox*>>& m_sandBoxStack;
    ToStringRecursionPreventer* m_toStringRecursionPreventer;
    VirtualIdentifierCallback m_virtualIdentifierCallback;
    SecurityPolicyCheckCallback m_securityPolicyCheckCallback;
    // public helper variable
    void* m_virtualIdentifierCallbackPublic;
    void* m_securityPolicyCheckCallbackPublic;
};
}

#endif
