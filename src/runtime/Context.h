/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

namespace Escargot {

class VMInstance;
class ScriptParser;
class ObjectStructure;
class ControlFlowRecord;
class SandBox;
class ByteCodeBlock;
class ToStringRecursionPreventer;
class FunctionTemplate;
class Debugger;

#if defined(ENABLE_WASM)
struct WASMCacheMap;
struct WASMHostFunctionEnvironment;
typedef Vector<WASMHostFunctionEnvironment*, GCUtil::gc_malloc_allocator<WASMHostFunctionEnvironment*>> WASMHostFunctionEnvironmentVector;
#endif

struct IdentifierRecord {
    AtomicString m_name;
    bool m_canDelete : 1;
    bool m_isMutable : 1;
    bool m_isVarDeclaration : 1;
};

typedef Vector<IdentifierRecord, GCUtil::gc_malloc_atomic_allocator<IdentifierRecord>> IdentifierRecordVector;
struct LoadedModule {
    Optional<Script*> m_referrer;
    Script* m_loadedModule;
    String* m_src;
};
typedef Vector<LoadedModule, GCUtil::gc_malloc_allocator<LoadedModule>> LoadedModuleVector;

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

typedef std::unordered_map<AtomicString, GlobalVariableAccessCacheItem*, std::hash<AtomicString>, std::equal_to<AtomicString>,
                           GCUtil::gc_malloc_allocator<std::pair<AtomicString const, GlobalVariableAccessCacheItem*>>>
    GlobalVariableAccessCache;

typedef Vector<std::pair<FunctionTemplate*, FunctionObject*>, GCUtil::gc_malloc_allocator<std::pair<FunctionTemplate*, FunctionObject*>>>
    InstantiatedFunctionObjects;

class Context : public gc {
    friend class AtomicString;
    friend class SandBox;
    friend class ByteCodeInterpreter;
    friend struct OpcodeTable;
    friend class ContextRef;
    friend class VirtualIdDisabler;
#if defined(ENABLE_CODE_CACHE)
    friend class CodeCacheWriter;
    friend class CodeCacheReader;
#endif

public:
    // Legacy RegExp Features (non-standard)
    struct RegExpLegacyFeatures {
        String* input; // RegExp.input ($_)
        StringView lastMatch;
        StringView lastParen;
        StringView leftContext;
        StringView rightContext;
        StringView dollars[9]; // RegExp.$1-$9
        size_t dollarCount;
        bool valid;

        RegExpLegacyFeatures()
            : input(String::emptyString)
            , lastMatch()
            , lastParen()
            , leftContext()
            , rightContext()
            , dollarCount(0)
            , valid(true)
        {
            for (size_t i = 0; i < 9; i++) {
                dollars[i] = StringView();
            }
        }

        bool isValid() const { return valid; }

        void invalidate()
        {
            input = String::emptyString;
            lastMatch = StringView();
            lastParen = StringView();
            leftContext = StringView();
            rightContext = StringView();
            for (size_t i = 0; i < 9; i++) {
                dollars[i] = StringView();
            }
            dollarCount = 0;
            valid = false;
        }
    };

    explicit Context(VMInstance* instance);

    VMInstance* vmInstance()
    {
        return m_instance;
    }

    StaticStrings& staticStrings()
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

#if defined(ENABLE_WASM)
    WASMCacheMap* wasmCache()
    {
        return m_wasmCache;
    }

    WASMHostFunctionEnvironmentVector* wasmEnvCache()
    {
        return m_wasmEnvCache;
    }
#endif

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

    ObjectStructure* defaultStructureForClassConstructorFunctionObjectWithName()
    {
        return m_defaultStructureForClassConstructorFunctionObjectWithName;
    }

    ObjectStructure* defaultStructureForStringObject()
    {
        return m_defaultStructureForStringObject;
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

    ObjectPrivateMemberStructure* defaultPrivateMemberStructure()
    {
        return m_defaultPrivateMemberStructure;
    }

    GlobalObject* globalObject()
    {
        return m_globalObject;
    }

    Object* globalObjectProxy()
    {
        return m_globalObjectProxy;
    }

    // this setter try to update `globalThis` value on GlobalObject
    void setGlobalObjectProxy(Object* newGlobalObjectProxy);

    ToStringRecursionPreventer* toStringRecursionPreventer()
    {
        return m_toStringRecursionPreventer;
    }

    void throwException(ExecutionState& state, const Value& exception);

    // this is not compatible with ECMAScript
    // but this callback is needed for browser-implementation
    // if there is a Identifier with that value, callback should return non-empty value
    void setVirtualIdentifierCallback(VirtualIdentifierCallback cb, void* callbackPublic)
    {
        m_virtualIdentifierCallback = cb;
        m_virtualIdentifierCallbackPublic = callbackPublic;
    }

    VirtualIdentifierCallback virtualIdentifierCallback()
    {
        return m_virtualIdentifierCallback;
    }

    void setSecurityPolicyCheckCallback(SecurityPolicyCheckCallback cb, void* callbackPublic)
    {
        m_securityPolicyCheckCallback = cb;
        m_securityPolicyCheckCallbackPublic = callbackPublic;
    }

    SecurityPolicyCheckCallback securityPolicyCheckCallback()
    {
        return m_securityPolicyCheckCallback;
    }

    IdentifierRecordVector* globalDeclarativeRecord()
    {
        return m_globalDeclarativeRecord;
    }

    EncodedValueVector* globalDeclarativeStorage()
    {
        return m_globalDeclarativeStorage;
    }

    GlobalVariableAccessCacheItem* ensureGlobalVariableAccessCacheSlot(AtomicString as);

    LoadedModuleVector* loadedModules()
    {
        return m_loadedModules;
    }

    RegExpLegacyFeatures& regexpLegacyFeatures()
    {
        return m_regexpLegacyFeatures;
    }

    InstantiatedFunctionObjects& instantiatedFunctionObjects()
    {
        return m_instantiatedFunctionObjects;
    }

    ASTAllocator& astAllocator();

#ifdef ESCARGOT_DEBUGGER
    Debugger* debugger()
    {
        return m_debugger;
    }

    bool initDebugger(const char* options);
    void printDebugger(StringView* output);
    String* getClientSource(String** sourceName);
#endif /* ESCARGOT_DEBUGGER */

private:
    VMInstance* m_instance;

    // these data actually store in VMInstance
    // we can store pointers of these data for reducing memory dereference to VMInstance
    AtomicStringMap* m_atomicStringMap;
    StaticStrings& m_staticStrings;
    GlobalObject* m_globalObject;
    // initially, this value is same with m_globalObject
    // this value indicates `this` value on source(global)
    Object* m_globalObjectProxy;
    ScriptParser* m_scriptParser;
    IdentifierRecordVector* m_globalDeclarativeRecord;
    EncodedValueVector* m_globalDeclarativeStorage;
    GlobalVariableAccessCache* m_globalVariableAccessCache;
    LoadedModuleVector* m_loadedModules;
    RegExpCacheMap* m_regexpCache;
#if defined(ENABLE_WASM)
    WASMCacheMap* m_wasmCache;
    WASMHostFunctionEnvironmentVector* m_wasmEnvCache;
#endif

    ObjectStructure* m_defaultStructureForObject;
    ObjectStructure* m_defaultStructureForFunctionObject;
    ObjectStructure* m_defaultStructureForNotConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForBuiltinFunctionObject;
    ObjectStructure* m_defaultStructureForFunctionPrototypeObject;
    ObjectStructure* m_defaultStructureForBoundFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObject;
    ObjectStructure* m_defaultStructureForClassConstructorFunctionObjectWithName;
    ObjectStructure* m_defaultStructureForStringObject;
    ObjectStructure* m_defaultStructureForRegExpObject;
    ObjectStructure* m_defaultStructureForMappedArgumentsObject;
    ObjectStructure* m_defaultStructureForUnmappedArgumentsObject;

    ObjectPrivateMemberStructure* m_defaultPrivateMemberStructure;

    ToStringRecursionPreventer* m_toStringRecursionPreventer;
    VirtualIdentifierCallback m_virtualIdentifierCallback;
    SecurityPolicyCheckCallback m_securityPolicyCheckCallback;
    // public helper variable
    void* m_virtualIdentifierCallbackPublic;
    void* m_securityPolicyCheckCallbackPublic;

    // For non-standard, read-only properties of RegExp
    // contains the result of the last matched regular expressions
    RegExpLegacyFeatures m_regexpLegacyFeatures;

    InstantiatedFunctionObjects m_instantiatedFunctionObjects;

#ifdef ESCARGOT_DEBUGGER
    // debugger support
    Debugger* m_debugger;
#endif /* ESCARGOT_DEBUGGER */
};

class VirtualIdDisabler {
public:
    explicit VirtualIdDisabler(Context* c)
        : m_fn(c->virtualIdentifierCallback())
        , m_fnPublic(c->m_virtualIdentifierCallbackPublic)
        , m_context(c)
    {
        c->setVirtualIdentifierCallback(nullptr, nullptr);
    }

    ~VirtualIdDisabler()
    {
        m_context->setVirtualIdentifierCallback(m_fn, m_fnPublic);
    }

    VirtualIdentifierCallback m_fn;
    void* m_fnPublic;
    Context* m_context;
};
} // namespace Escargot

#endif
