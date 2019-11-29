/*
 * Copyright (c) 2018-present Samsung Electronics Co., Ltd
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
#include "ModuleNamespaceObject.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/VMInstance.h"
#include "runtime/Context.h"
#include "parser/Script.h"

namespace Escargot {

ModuleNamespaceObject::ModuleNamespaceObject(ExecutionState& state, ModuleEnvironmentRecord* record)
    : Object(state)
    , m_isInitialized(false)
    , m_moduleEnvironmentRecord(record)
{
    // Let exportedNames be module.GetExportedNames(« »).
    auto exportedNames = m_moduleEnvironmentRecord->script()->exportedNames(state);
    // ReturnIfAbrupt(exportedNames).
    // Let unambiguousNames be a new empty List.
    AtomicStringVector unambiguousNames;
    // For each name that is an element of exportedNames,
    for (size_t i = 0; i < exportedNames.size(); i++) {
        // Let resolution be module.ResolveExport(name, « », « »).
        auto resolution = m_moduleEnvironmentRecord->script()->resolveExport(state, exportedNames[i]);
        // ReturnIfAbrupt(resolution).
        // If resolution is null, throw a SyntaxError exception.
        if (resolution.m_type == Script::ResolveExportResult::Null) {
            // TODO give better error message
            ErrorObject::throwBuiltinError(state, ErrorObject::Code::SyntaxError, "Exported name '%s' is null", exportedNames[i]);
        }
        // If resolution is not "ambiguous", append name to unambiguousNames.
        if (resolution.m_type != Script::ResolveExportResult::Ambiguous) {
            unambiguousNames.push_back(exportedNames[i]);
        }
    }
    // Let namespace be ModuleNamespaceCreate(module, unambiguousNames).
    m_exports = std::move(unambiguousNames);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-@@tostringtag
    Object::defineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)), ObjectPropertyDescriptor(Value(state.context()->staticStrings().Module.string()), ObjectPropertyDescriptor::ConfigurablePresent));

    m_isInitialized = true;
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p
ObjectGetResult ModuleNamespaceObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto pAsPropertyName = P.toObjectStructurePropertyName(state);
    // If Type(P) is Symbol, return OrdinaryGetOwnProperty(O, P).
    if (pAsPropertyName.isSymbol()) {
        return Object::getOwnProperty(state, P);
    }
    // Let exports be the value of O’s [[Exports]] internal slot.
    for (size_t i = 0; i < m_exports.size(); i++) {
        if (pAsPropertyName == ObjectStructurePropertyName(m_exports[i])) {
            // Let value be O.[[Get]](P, O).
            Value value = this->get(state, P).value(state, this);
            // ReturnIfAbrupt(value).
            // Return PropertyDescriptor{[[Value]]: value, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: false }.
            return ObjectGetResult(value, true, true, false);
        }
    }
    // If P is not an element of exports, return undefined.
    return ObjectGetResult();
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-hasproperty-p
ObjectHasPropertyResult ModuleNamespaceObject::hasProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    // If Type(P) is Symbol, return OrdinaryHasProperty(O, P).
    auto pAsPropertyName = P.toObjectStructurePropertyName(state);
    // If Type(P) is Symbol, return OrdinaryGetOwnProperty(O, P).
    if (pAsPropertyName.isSymbol()) {
        return Object::hasProperty(state, P);
    }
    // Let exports be the value of O’s [[Exports]] internal slot.
    // If P is an element of exports, return true.
    // Return false.
    for (size_t i = 0; i < m_exports.size(); i++) {
        if (pAsPropertyName == ObjectStructurePropertyName(m_exports[i])) {
            return ObjectHasPropertyResult([](ExecutionState& state, const ObjectPropertyName& P, void* handlerData) -> Value {
                ModuleNamespaceObject* p = (ModuleNamespaceObject*)handlerData;
                return p->getOwnProperty(state, P).value(state, p);
            },
                                           this);
        }
    }
    return ObjectHasPropertyResult();
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-get-p-receiver
ObjectGetResult ModuleNamespaceObject::get(ExecutionState& state, const ObjectPropertyName& P)
{
    // Assert: IsPropertyKey(P) is true.
    // If Type(P) is Symbol, then
    auto pAsPropertyName = P.toObjectStructurePropertyName(state);
    if (pAsPropertyName.isSymbol()) {
        // Return the result of calling the default ordinary object [[Get]] internal method (9.1.8) on O passing P and Receiver as arguments.
        return Object::get(state, P);
    }
    // Let exports be the value of O’s [[Exports]] internal slot.
    for (size_t i = 0; i < m_exports.size(); i++) {
        if (pAsPropertyName == ObjectStructurePropertyName(m_exports[i])) {
            AtomicString pAsAtomicString(state, pAsPropertyName.plainString());
            // Let m be the value of O’s [[Module]] internal slot.
            // Let binding be m.ResolveExport(P, «», «»).
            auto binding = m_moduleEnvironmentRecord->script()->resolveExport(state, pAsAtomicString);
            // ReturnIfAbrupt(binding).
            // Assert: binding is neither null nor "ambiguous".
            // Let targetModule be binding.[[module]],
            Script* targetModule = std::get<0>(binding.m_record.value());
            // Assert: targetModule is not undefined.
            // Let targetEnv be targetModule.[[Environment]].
            if (!targetModule->moduleData()->m_moduleRecord) {
                // If targetEnv is undefined, throw a ReferenceError exception.
                ErrorObject::throwBuiltinError(state, ErrorObject::Code::ReferenceError, "module '%s' is not correctly loaded", targetModule->src());
            }
            // Let targetEnvRec be targetEnv’s EnvironmentRecord.
            // Return targetEnvRec.GetBindingValue(binding.[[bindingName]], true).
            auto bindingResult = targetModule->moduleData()->m_moduleRecord->getBindingValue(state, std::get<1>(binding.m_record.value()));
            ASSERT(bindingResult.m_hasBindingValue);
            return ObjectGetResult(bindingResult.m_value, true, true, false);
        }
    }
    // If P is not an element of exports, return undefined.
    return ObjectGetResult();
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-delete-p
bool ModuleNamespaceObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P)
{
    auto pAsPropertyName = P.toObjectStructurePropertyName(state);
    // Let exports be the value of O’s [[Exports]] internal slot.
    // If P is an element of exports, return true.
    // Return false.
    for (size_t i = 0; i < m_exports.size(); i++) {
        if (pAsPropertyName == ObjectStructurePropertyName(m_exports[i])) {
            return false;
        }
    }
    return true;
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-ownpropertykeys
void ModuleNamespaceObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data, bool shouldSkipSymbolKey)
{
    Object::enumeration(state, callback, data, shouldSkipSymbolKey);

    for (size_t i = 0; i < m_exports.size(); i++) {
        callback(state, this, ObjectPropertyName(m_exports[i]),
                 ObjectStructurePropertyDescriptor::createDataDescriptor((ObjectStructurePropertyDescriptor::PresentAttribute)(ObjectStructurePropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)), data);
    }
}
}
