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

#include "Escargot.h"
#include "Context.h"
#include "runtime/AtomicString.h"
#include "runtime/ThreadLocal.h"
#include "VMInstance.h"
#include "GlobalObject.h"
#include "StringObject.h"
#include "parser/ScriptParser.h"
#include "ObjectStructure.h"
#include "Environment.h"
#include "EnvironmentRecord.h"
#include "parser/CodeBlock.h"
#include "SandBox.h"
#include "ArrayObject.h"
#include "debugger/Debugger.h"
#if defined(ENABLE_WASM)
#include "wasm/WASMObject.h"
#endif

namespace Escargot {

void* GlobalVariableAccessCacheItem::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(GlobalVariableAccessCacheItem)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(GlobalVariableAccessCacheItem, m_cachedStructure));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(GlobalVariableAccessCacheItem));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Context::Context(VMInstance* instance)
    : m_instance(instance)
    , m_atomicStringMap(&instance->m_atomicStringMap)
    , m_staticStrings(instance->m_staticStrings)
    , m_scriptParser(new ScriptParser(this))
    , m_globalDeclarativeRecord(new IdentifierRecordVector())
    , m_globalDeclarativeStorage(new EncodedValueVector())
    , m_globalVariableAccessCache(new (GC) GlobalVariableAccessCache)
    , m_loadedModules(new LoadedModuleVector())
    , m_regexpCache(instance->m_regexpCache)
#if defined(ENABLE_WASM)
    , m_wasmCache(new WASMCacheMap())
    , m_wasmEnvCache(new WASMHostFunctionEnvironmentVector())
#endif

    , m_defaultStructureForObject(instance->m_defaultStructureForObject)
    , m_defaultStructureForFunctionObject(instance->m_defaultStructureForFunctionObject)
    , m_defaultStructureForNotConstructorFunctionObject(instance->m_defaultStructureForNotConstructorFunctionObject)
    , m_defaultStructureForBuiltinFunctionObject(instance->m_defaultStructureForBuiltinFunctionObject)
    , m_defaultStructureForFunctionPrototypeObject(instance->m_defaultStructureForFunctionPrototypeObject)
    , m_defaultStructureForBoundFunctionObject(instance->m_defaultStructureForBoundFunctionObject)
    , m_defaultStructureForClassConstructorFunctionObject(instance->m_defaultStructureForClassConstructorFunctionObject)
    , m_defaultStructureForClassConstructorFunctionObjectWithName(instance->m_defaultStructureForClassConstructorFunctionObjectWithName)
    , m_defaultStructureForStringObject(instance->m_defaultStructureForStringObject)
    , m_defaultStructureForRegExpObject(instance->m_defaultStructureForRegExpObject)
    , m_defaultStructureForMappedArgumentsObject(instance->m_defaultStructureForMappedArgumentsObject)
    , m_defaultStructureForUnmappedArgumentsObject(instance->m_defaultStructureForUnmappedArgumentsObject)
    , m_defaultPrivateMemberStructure(instance->m_defaultPrivateMemberStructure)
    , m_toStringRecursionPreventer(&instance->m_toStringRecursionPreventer)
    , m_virtualIdentifierCallback(nullptr)
    , m_securityPolicyCheckCallback(nullptr)
    , m_virtualIdentifierCallbackPublic(nullptr)
    , m_securityPolicyCheckCallbackPublic(nullptr)
#ifdef ESCARGOT_DEBUGGER
    , m_debugger(nullptr)
#endif /* ESCARGOT_DEBUGGER */
{
    ExecutionState stateForInit(this);
    m_globalObjectProxy = m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->initializeBuiltins(stateForInit);
}

void Context::throwException(ExecutionState& state, const Value& exception)
{
    if (LIKELY(vmInstance()->currentSandBox() != nullptr)) {
        vmInstance()->currentSandBox()->throwException(state, exception);
    } else {
        ESCARGOT_LOG_ERROR("there is no sandbox but exception occurred");
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void Context::setGlobalObjectProxy(Object* newGlobalObjectProxy)
{
    m_globalObjectProxy = newGlobalObjectProxy;

    // this setter try to update `globalThis` value on GlobalObject
    SandBox sb(this);
    sb.run([](ExecutionState& state, void* data) -> Value {
        Object* newGlobalObjectProxy = (Object*)data;
        state.context()->globalObject()->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().globalThis),
                                                           ObjectPropertyDescriptor(newGlobalObjectProxy, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NotPresent)));
        return Value();
    },
           newGlobalObjectProxy);
}

ASTAllocator& Context::astAllocator()
{
    return *ThreadLocal::astAllocator();
}

#ifdef ESCARGOT_DEBUGGER

bool Context::initDebugger(const char* options)
{
    if (m_debugger != nullptr) {
        // debugger cannot be re-initialized
        return false;
    }

    m_debugger = createDebugger(options, &m_instance->m_debuggerEnabled);
    return m_debugger->enabled();
}

void Context::printDebugger(StringView* output)
{
    if (m_debugger != nullptr) {
        m_debugger->consoleOut(output);
    }
}

String* Context::getClientSource(String** sourceName)
{
    if (m_debugger != nullptr) {
        return m_debugger->getClientSource(sourceName);
    }
    return nullptr;
}

#endif /* ESCARGOT_DEBUGGER */

GlobalVariableAccessCacheItem* Context::ensureGlobalVariableAccessCacheSlot(AtomicString as)
{
    auto iter = m_globalVariableAccessCache->find(as);
    if (iter == m_globalVariableAccessCache->end()) {
        GlobalVariableAccessCacheItem* slot = new GlobalVariableAccessCacheItem();
        slot->m_lexicalIndexCache = std::numeric_limits<size_t>::max();
        slot->m_propertyName = as;
        slot->m_cachedAddress = nullptr;
        slot->m_cachedStructure = nullptr;
        m_globalVariableAccessCache->insert(std::make_pair(as, slot));
        return slot;
    }

    return iter->second;
}
} // namespace Escargot
