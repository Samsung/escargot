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

#include "Escargot.h"
#include "Context.h"
#include "runtime/AtomicString.h"
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

namespace Escargot {

void* GlobalVariableAccessCacheItem::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
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
    , m_globalDeclarativeStorage(new SmallValueVector())
    , m_globalVariableAccessCache(new (GC) GlobalVariableAccessCache)
    , m_loadedModules(new LoadedModuleVector())
    , m_bumpPointerAllocator(instance->m_bumpPointerAllocator)
    , m_regexpCache(instance->m_regexpCache)
    , m_toStringRecursionPreventer(&instance->m_toStringRecursionPreventer)
    , m_astAllocator(*instance->m_astAllocator)
#ifdef ESCARGOT_DEBUGGER
    , m_debugger(nullptr)
#endif /* ESCARGOT_DEBUGGER */
{
    m_defaultStructureForObject = m_instance->m_defaultStructureForObject;
    m_defaultStructureForFunctionObject = m_instance->m_defaultStructureForFunctionObject;
    m_defaultStructureForNotConstructorFunctionObject = m_instance->m_defaultStructureForNotConstructorFunctionObject;
    m_defaultStructureForBuiltinFunctionObject = m_instance->m_defaultStructureForBuiltinFunctionObject;
    m_defaultStructureForFunctionPrototypeObject = m_instance->m_defaultStructureForFunctionPrototypeObject;
    m_defaultStructureForBoundFunctionObject = m_instance->m_defaultStructureForBoundFunctionObject;
    m_defaultStructureForClassConstructorFunctionObject = m_instance->m_defaultStructureForClassConstructorFunctionObject;
    m_defaultStructureForStringObject = m_instance->m_defaultStructureForStringObject;
    m_defaultStructureForSymbolObject = m_instance->m_defaultStructureForSymbolObject;
    m_defaultStructureForRegExpObject = m_instance->m_defaultStructureForRegExpObject;
    m_defaultStructureForMappedArgumentsObject = m_instance->m_defaultStructureForMappedArgumentsObject;
    m_defaultStructureForUnmappedArgumentsObject = m_instance->m_defaultStructureForUnmappedArgumentsObject;

    m_virtualIdentifierCallback = nullptr;
    m_securityPolicyCheckCallback = nullptr;
    m_virtualIdentifierCallbackPublic = nullptr;
    m_securityPolicyCheckCallbackPublic = nullptr;

    ExecutionState stateForInit(this);
    m_globalObject = new GlobalObject(stateForInit);
    m_globalObject->installBuiltins(stateForInit);

    ArrayObject temp(stateForInit);
    g_arrayObjectTag = *((size_t*)&temp);
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
}
