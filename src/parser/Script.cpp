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
#include "Script.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "parser/ast/Node.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/SandBox.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ModuleNamespaceObject.h"
#include "util/Util.h"
#include "parser/ast/AST.h"

namespace Escargot {

bool Script::isExecuted()
{
    return m_topCodeBlock->m_byteCodeBlock == nullptr;
}

Context* Script::context()
{
    return m_topCodeBlock->context();
}

static Optional<Script*> findLoadedModule(Context* context, Optional<Script*> referrer, String* src)
{
    const auto& lm = *context->loadedModules();
    for (size_t j = 0; j < lm.size(); j++) {
        if (lm[j].m_referrer == referrer && lm[j].m_src->equals(src)) {
            return Optional<Script*>(lm[j].m_loadedModule);
        }
    }

    return Optional<Script*>();
}

static void registerToLoadedModuleIfNeeds(Context* context, Optional<Script*> referrer, String* src, Script* module)
{
    if (!findLoadedModule(context, referrer, src)) {
        LoadedModule m;
        m.m_loadedModule = module;
        m.m_referrer = referrer;
        m.m_src = src;
        auto lm = context->loadedModules();
        lm->push_back(m);
    }
}

void Script::loadExternalModule(ExecutionState& state)
{
    for (size_t i = 0; i < m_moduleData->m_importEntries.size(); i++) {
        bool loaded = findLoadedModule(context(), this, m_moduleData->m_importEntries[i].m_moduleRequest).hasValue();
        if (!loaded) {
            loadModuleFromScript(state, m_moduleData->m_importEntries[i].m_moduleRequest);
        }
    }

    for (size_t i = 0; i < m_moduleData->m_indirectExportEntries.size(); i++) {
        bool loaded = findLoadedModule(context(), this, m_moduleData->m_indirectExportEntries[i].m_moduleRequest.value()).hasValue();
        if (!loaded) {
            loadModuleFromScript(state, m_moduleData->m_indirectExportEntries[i].m_moduleRequest.value());
        }
    }

    for (size_t i = 0; i < m_moduleData->m_starExportEntries.size(); i++) {
        bool loaded = findLoadedModule(context(), this, m_moduleData->m_starExportEntries[i].m_moduleRequest.value()).hasValue();
        if (!loaded) {
            loadModuleFromScript(state, m_moduleData->m_starExportEntries[i].m_moduleRequest.value());
        }
    }
}

void Script::loadModuleFromScript(ExecutionState& state, String* src)
{
    Platform::LoadModuleResult result = context()->vmInstance()->platform()->onLoadModule(context(), this, src);
    if (!result.script) {
        ErrorObject::throwBuiltinError(state, (ErrorObject::Code)result.errorCode, result.errorMessage->toNonGCUTF8StringData().data());
    }
    registerToLoadedModuleIfNeeds(context(), this, src, result.script.value());
    result.script->loadExternalModule(state);
}

size_t Script::moduleRequestsLength()
{
    if (!isModule()) {
        return 0;
    }
    return m_moduleData->m_importEntries.size() + m_moduleData->m_indirectExportEntries.size() + m_moduleData->m_starExportEntries.size();
}

String* Script::moduleRequest(size_t i)
{
    if (!isModule()) {
        return String::emptyString;
    }

    if (i < m_moduleData->m_importEntries.size()) {
        return m_moduleData->m_importEntries[i].m_moduleRequest;
    }

    i = i - m_moduleData->m_importEntries.size();

    if (i < m_moduleData->m_indirectExportEntries.size()) {
        return m_moduleData->m_indirectExportEntries[i].m_moduleRequest.value();
    }

    i = i - m_moduleData->m_indirectExportEntries.size();

    return m_moduleData->m_starExportEntries[i].m_moduleRequest.value();
}

AtomicStringVector Script::exportedNames(ExecutionState& state, std::vector<Script*>& exportStarSet)
{
    // Let module be this Source Text Module Record.
    Script* module = this;
    // If exportStarSet contains module, then
    for (size_t i = 0; i < exportStarSet.size(); i++) {
        if (exportStarSet[i] == module) {
            // Assert: We’ve reached the starting point of an import * circularity.
            // Return a new empty List.
            return AtomicStringVector();
        }
    }

    // Append module to exportStarSet.
    exportStarSet.push_back(module);
    // Let exportedNames be a new empty List.
    AtomicStringVector exportedNames;
    // For each ExportEntry Record e in module.[[LocalExportEntries]], do
    auto& localExportEntries = m_moduleData->m_localExportEntries;
    for (size_t i = 0; i < localExportEntries.size(); i++) {
        auto& e = localExportEntries[i];
        // Assert: module provides the direct binding for this export.
        // Append e.[[ExportName]] to exportedNames.
        exportedNames.push_back(e.m_exportName.value());
    }

    // For each ExportEntry Record e in module.[[IndirectExportEntries]], do
    auto& indirectExportEntries = m_moduleData->m_indirectExportEntries;
    for (size_t i = 0; i < indirectExportEntries.size(); i++) {
        auto& e = indirectExportEntries[i];
        // Assert: module imports a specific binding for this export.
        // Append e.[[ExportName]] to exportedNames.
        exportedNames.push_back(e.m_exportName.value());
    }

    // For each ExportEntry Record e in module.[[StarExportEntries]], do
    auto& starExportEntries = m_moduleData->m_starExportEntries;
    for (size_t i = 0; i < starExportEntries.size(); i++) {
        auto& e = starExportEntries[i];

        // Let requestedModule be HostResolveImportedModule(module, e.[[ModuleRequest]]).
        // ReturnIfAbrupt(requestedModule).
        Script* requestedModule = findLoadedModule(context(), this, e.m_moduleRequest.value()).value();

        // Let starNames be requestedModule.GetExportedNames(exportStarSet).
        auto starNames = requestedModule->exportedNames(state, exportStarSet);

        // For each element n of starNames, do
        for (size_t i = 0; i < starNames.size(); i++) {
            // If SameValue(n, "default") is false, then
            if (starNames[i] != state.context()->staticStrings().stringDefault) {
                // If n is not an element of exportedNames, then
                bool found = false;
                for (size_t j = 0; j < exportedNames.size(); j++) {
                    if (starNames[i] == exportedNames[j]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Append n to exportedNames.
                    exportedNames.push_back(starNames[i]);
                }
            }
        }
    }

    // Return exportedNames.
    return exportedNames;
}

Script::ResolveExportResult Script::resolveExport(ExecutionState& state, AtomicString exportName, std::vector<std::tuple<Script*, AtomicString>>& resolveSet, std::vector<Script*>& exportStarSet)
{
    ASSERT(isModule());
    // Let module be this Source Text Module Record.
    Script* module = this;
    // For each Record {[[module]], [[exportName]]} r in resolveSet, do:
    for (size_t i = 0; i < resolveSet.size(); i++) {
        // If module and r.[[module]] are the same Module Record and SameValue(exportName, r.[[exportName]]) is true, then
        if (std::get<0>(resolveSet[i]) == module && std::get<1>(resolveSet[i]) == exportName) {
            // Assert: this is a circular import request.
            // Return null.
            return Script::ResolveExportResult(Script::ResolveExportResult::Null);
        }
    }

    // Append the Record {[[module]]: module, [[exportName]]: exportName} to resolveSet.
    resolveSet.push_back(std::make_tuple(this, exportName));

    // For each ExportEntry Record e in module.[[LocalExportEntries]], do
    auto& localExportEntries = m_moduleData->m_localExportEntries;
    for (size_t i = 0; i < localExportEntries.size(); i++) {
        // If SameValue(exportName, e.[[ExportName]]) is true, then
        if (localExportEntries[i].m_exportName == exportName) {
            // Assert: module provides the direct binding for this export.
            // Return Record{[[module]]: module, [[bindingName]]: e.[[LocalName]]}.
            return Script::ResolveExportResult(Script::ResolveExportResult::Record, Optional<std::tuple<Script*, AtomicString>>(std::make_tuple(module, localExportEntries[i].m_localName.value())));
        }
    }

    // For each ExportEntry Record e in module.[[IndirectExportEntries]], do
    auto& indirectExportEntries = m_moduleData->m_indirectExportEntries;
    for (size_t i = 0; i < indirectExportEntries.size(); i++) {
        auto& e = indirectExportEntries[i];
        // If SameValue(exportName, e.[[ExportName]]) is true, then
        if (e.m_exportName == exportName) {
            // Assert: module imports a specific binding for this export.
            // Let importedModule be HostResolveImportedModule(module, e.[[ModuleRequest]]).
            // ReturnIfAbrupt(importedModule).
            Script* importedModule = findLoadedModule(context(), this, e.m_moduleRequest.value()).value();
            // Let indirectResolution be importedModule.ResolveExport(e.[[ImportName]], resolveSet, exportStarSet).
            auto indirectResolution = importedModule->resolveExport(state, e.m_importName.value(), resolveSet, exportStarSet);
            // ReturnIfAbrupt(indirectResolution).
            // If indirectResolution is not null, return indirectResolution.
            if (indirectResolution.m_type != Script::ResolveExportResult::Null) {
                return indirectResolution;
            }
        }
    }

    // If SameValue(exportName, "default") is true, then
    if (exportName == context()->staticStrings().stringDefault) {
        // Assert: A default export was not explicitly defined by this module.
        // Throw a SyntaxError exception.
        // NOTE A default export cannot be provided by an export *.
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::SyntaxError, "The module '%s' does not provide an export named 'default'", src());
    }

    // If exportStarSet contains module, then return null.
    for (size_t i = 0; i < exportStarSet.size(); i++) {
        if (exportStarSet[i] == module) {
            return Script::ResolveExportResult(Script::ResolveExportResult::Null);
        }
    }
    // Append module to exportStarSet.
    exportStarSet.push_back(module);
    // Let starResolution be null.
    Script::ResolveExportResult starResolution(Script::ResolveExportResult::Null);

    // For each ExportEntry Record e in module.[[StarExportEntries]], do
    auto& starExportEntries = m_moduleData->m_starExportEntries;
    for (size_t i = 0; i < starExportEntries.size(); i++) {
        auto& e = starExportEntries[i];
        // Let importedModule be HostResolveImportedModule(module, e.[[ModuleRequest]]).
        // ReturnIfAbrupt(importedModule).
        Script* importedModule = findLoadedModule(context(), this, e.m_moduleRequest.value()).value();
        // Let resolution be importedModule.ResolveExport(exportName, resolveSet, exportStarSet).
        // ReturnIfAbrupt(resolution).
        auto resolution = importedModule->resolveExport(state, exportName, resolveSet, exportStarSet);
        // If resolution is "ambiguous", return "ambiguous".
        if (resolution.m_type == Script::ResolveExportResult::Ambiguous) {
            return resolution;
        }
        // If resolution is not null, then
        if (resolution.m_type != Script::ResolveExportResult::Null) {
            // If starResolution is null, let starResolution be resolution.
            if (starResolution.m_type == Script::ResolveExportResult::Null) {
                starResolution = resolution;
            } else {
                // Else
                // Assert: there is more than one * import that includes the requested name.
                // If resolution.[[module]] and starResolution.[[module]] are not the same Module Record or SameValue(resolution.[[exportName]], starResolution.[[exportName]]) is false, return "ambiguous".
                if (std::get<0>(resolution.m_record.value()) != std::get<0>(starResolution.m_record.value())
                    || std::get<1>(resolution.m_record.value()) != std::get<1>(starResolution.m_record.value())) {
                    return Script::ResolveExportResult(Script::ResolveExportResult::Ambiguous);
                }
            }
        }
    }

    return starResolution;
}

Value Script::executeModule(ExecutionState& state, Optional<Script*> referrer)
{
    ByteCodeBlock* byteCodeBlock = m_topCodeBlock->byteCodeBlock();

    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduledeclarationinstantiation
    // ModuleDeclarationInstantiation( ) Concrete Method
    LexicalEnvironment* globalLexicalEnv = new LexicalEnvironment(
        new GlobalEnvironmentRecord(state, nullptr, context()->globalObject(), context()->globalDeclarativeRecord(), context()->globalDeclarativeStorage()), nullptr);

    ExecutionState newState(context(), state.stackLimit());

    ModuleEnvironmentRecord* moduleRecord = new ModuleEnvironmentRecord(this);
    m_moduleData->m_moduleRecord = moduleRecord;

    context()->vmInstance()->platform()->didLoadModule(context(), referrer, this);
    registerToLoadedModuleIfNeeds(context(), referrer, src(), this);
    loadExternalModule(state);

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        moduleRecord->createBinding(newState, vec[i].m_name, false, vec[i].m_isMutable, true);
    }

    InterpretedCodeBlock::BlockInfo* bi = m_topCodeBlock->blockInfo(0);
    if (bi) {
        len = bi->m_identifiers.size();
        for (size_t i = 0; i < len; i++) {
            moduleRecord->createBinding(newState, bi->m_identifiers[i].m_name, false, bi->m_identifiers[i].m_isMutable, false);
        }
    }

    // Test import, export first
    // For each ImportEntry Record in in module.[[ImportEntries]], do
    for (size_t i = 0; i < m_moduleData->m_importEntries.size(); i++) {
        auto& in = m_moduleData->m_importEntries[i];
        // Let importedModule be HostResolveImportedModule(module, in.[[ModuleRequest]]).
        // ReturnIfAbrupt(importedModule).
        Script* importedModule = findLoadedModule(context(), this, in.m_moduleRequest).value();

        // If in.[[ImportName]] is "*", then
        if (in.m_importName == context()->staticStrings().asciiTable[(unsigned char)'*']) {
        } else {
            // Let resolution be importedModule.ResolveExport(in.[[ImportName]], « », «‍ »).
            auto resolution = importedModule->resolveExport(newState, in.m_importName);
            // ReturnIfAbrupt(resolution).
            // If resolution is null or resolution is "ambiguous", throw a SyntaxError exception.
            // Call envRec.CreateImportBinding(in.[[LocalName]], resolution.[[module]], resolution.[[bindingName]]).
            if (resolution.m_type == Script::ResolveExportResult::Null) {
                StringBuilder builder;
                builder.appendString("The requested module '");
                builder.appendString(in.m_moduleRequest);
                builder.appendString("' does not provide an export named '");
                builder.appendString(in.m_localName.string());
                builder.appendString("'");
                ErrorObject::throwBuiltinError(newState, ErrorObject::Code::SyntaxError, builder.finalize(&newState)->toNonGCUTF8StringData().data());
            } else if (resolution.m_type == Script::ResolveExportResult::Ambiguous) {
                StringBuilder builder;
                builder.appendString("The requested module '");
                builder.appendString(in.m_moduleRequest);
                builder.appendString("' does not provide an export named '");
                builder.appendString(in.m_localName.string());
                builder.appendString("' correctly");
                ErrorObject::throwBuiltinError(newState, ErrorObject::Code::SyntaxError, builder.finalize(&newState)->toNonGCUTF8StringData().data());
            }
        }
    }

    // execute external modules
    for (size_t i = 0; i < m_moduleData->m_importEntries.size(); i++) {
        Script* script = findLoadedModule(context(), this, m_moduleData->m_importEntries[i].m_moduleRequest).value();
        if (!script->isExecuted()) {
            script->executeModule(state, this);
        }
    }

    for (size_t i = 0; i < m_moduleData->m_indirectExportEntries.size(); i++) {
        Script* script = findLoadedModule(context(), this, m_moduleData->m_indirectExportEntries[i].m_moduleRequest.value()).value();
        if (!script->isExecuted()) {
            script->executeModule(state, this);
        }
    }

    for (size_t i = 0; i < m_moduleData->m_starExportEntries.size(); i++) {
        Script* script = findLoadedModule(context(), this, m_moduleData->m_starExportEntries[i].m_moduleRequest.value()).value();
        if (!script->isExecuted()) {
            script->executeModule(state, this);
        }
    }

    // Import variables
    // For each ImportEntry Record in in module.[[ImportEntries]], do
    for (size_t i = 0; i < m_moduleData->m_importEntries.size(); i++) {
        auto& in = m_moduleData->m_importEntries[i];
        // Let importedModule be HostResolveImportedModule(module, in.[[ModuleRequest]]).
        // ReturnIfAbrupt(importedModule).
        Script* importedModule = findLoadedModule(context(), this, in.m_moduleRequest).value();

        // If in.[[ImportName]] is "*", then
        if (in.m_importName == context()->staticStrings().asciiTable[(unsigned char)'*']) {
            // Let namespace be GetModuleNamespace(importedModule).
            // ReturnIfAbrupt(module).
            ModuleNamespaceObject* namespaceObject = importedModule->moduleData()->m_moduleRecord->namespaceObject(newState);
            // Let status be envRec.CreateImmutableBinding(in.[[LocalName]], true).
            // Assert: status is not an abrupt completion.
            // Call envRec.InitializeBinding(in.[[LocalName]], namespace).
            moduleRecord->initializeBinding(newState, in.m_localName, Value(namespaceObject));
        } else {
            // Let resolution be importedModule.ResolveExport(in.[[ImportName]], « », «‍ »).
            auto resolution = importedModule->resolveExport(newState, in.m_importName);
            // ReturnIfAbrupt(resolution).
            // If resolution is null or resolution is "ambiguous", throw a SyntaxError exception.
            // Call envRec.CreateImportBinding(in.[[LocalName]], resolution.[[module]], resolution.[[bindingName]]).
            ASSERT(resolution.m_type == Script::ResolveExportResult::Record);
            auto bindingResult = std::get<0>(resolution.m_record.value())->moduleData()->m_moduleRecord->getBindingValue(newState, std::get<1>(resolution.m_record.value()));
            ASSERT(bindingResult.m_hasBindingValue);
            moduleRecord->initializeBinding(newState, in.m_localName, bindingResult.m_value);
        }
    }

    newState.setLexicalEnvironment(new LexicalEnvironment(moduleRecord, globalLexicalEnv), true);

    size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    Value* registerFile = (Value*)ALLOCA((byteCodeBlock->m_requiredRegisterFileSizeInValueSize + 1 + literalStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth()) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
    stackStorage[0] = Value();
    Value* literalStorage = stackStorage + 1 + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue = ByteCodeInterpreter::interpret(&newState, byteCodeBlock, 0, registerFile);
    clearStack<512>();

    // we give up program bytecodeblock after first excution for reducing memory usage
    m_topCodeBlock->m_byteCodeBlock = nullptr;

    return resultValue;
}

// https://www.ecma-international.org/ecma-262/9.0/#sec-candeclareglobalfunction
static bool canDeclareGlobalFunction(ExecutionState& state, Object* globalObject, AtomicString N)
{
    // Let envRec be the global Environment Record for which the method was invoked.
    // Let ObjRec be envRec.[[ObjectRecord]].
    // Let globalObject be the binding object for ObjRec.
    // Let existingProp be ? globalObject.[[GetOwnProperty]](N).
    auto existingProp = globalObject->getOwnProperty(state, N);
    // If existingProp is undefined, return ? IsExtensible(globalObject).
    if (!existingProp.hasValue()) {
        return globalObject->isExtensible(state);
    }
    // If existingProp.[[Configurable]] is true, return true.
    if (existingProp.isConfigurable()) {
        return true;
    }
    // If IsDataDescriptor(existingProp) is true and existingProp has attribute values { [[Writable]]: true, [[Enumerable]]: true }, return true.
    if (existingProp.isDataProperty() && existingProp.isWritable() && existingProp.isEnumerable()) {
        return true;
    }
    // Return false.
    return false;
}

static void testDeclareGlobalFunctions(ExecutionState& state, InterpretedCodeBlock* topCodeBlock, Object* globalObject)
{
    auto c = topCodeBlock->firstChild();
    while (c) {
        if (c->isFunctionDeclaration() && c->lexicalBlockIndexFunctionLocatedIn() == 0) {
            if (!canDeclareGlobalFunction(state, globalObject, c->functionName())) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Identifier '%s' has already been declared", c->functionName());
            }
        }
        c = c->nextSibling();
    }
}

Value Script::execute(ExecutionState& state, bool isExecuteOnEvalFunction, bool inStrictMode)
{
    if (UNLIKELY(isExecuted())) {
        if (!m_canExecuteAgain) {
            ESCARGOT_LOG_ERROR("You cannot re-execute this type of Script object");
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_topCodeBlock = state.context()->scriptParser().initializeScript(m_sourceCode, m_src, m_moduleData).script->m_topCodeBlock;
    }

    if (isModule()) {
        return executeModule(state, nullptr);
    }

    ByteCodeBlock* byteCodeBlock = m_topCodeBlock->byteCodeBlock();

    ExecutionState newState(context(), state.stackLimit());
    ExecutionState* codeExecutionState = &newState;

    EnvironmentRecord* globalRecord = new GlobalEnvironmentRecord(state, m_topCodeBlock, context()->globalObject(), context()->globalDeclarativeRecord(), context()->globalDeclarativeStorage());
    LexicalEnvironment* globalLexicalEnvironment = new LexicalEnvironment(globalRecord, nullptr);
    newState.setLexicalEnvironment(globalLexicalEnvironment, m_topCodeBlock->isStrict());

    EnvironmentRecord* globalVariableRecord = globalRecord;

    if (isExecuteOnEvalFunction) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        // + Indirect eval code creates a new declarative environment for lexically-scoped declarations (let)
        EnvironmentRecord* newVariableRecord = new DeclarativeEnvironmentRecordNotIndexed(state, true);
        ExecutionState* newVariableState = new ExecutionState(context());
        newVariableState->setLexicalEnvironment(new LexicalEnvironment(newVariableRecord, globalLexicalEnvironment), m_topCodeBlock->isStrict());
        newVariableState->setParent(&newState);
        codeExecutionState = newVariableState;
        if (inStrictMode) {
            globalVariableRecord = newVariableRecord;
        }
    }

    testDeclareGlobalFunctions(state, m_topCodeBlock, context()->globalObject());

    const InterpretedCodeBlock::IdentifierInfoVector& identifierVector = m_topCodeBlock->identifierInfos();
    size_t identifierVectorLen = identifierVector.size();

    const auto& globalLexicalVector = m_topCodeBlock->blockInfo(0)->m_identifiers;
    size_t globalLexicalVectorLen = globalLexicalVector.size();

    if (!isExecuteOnEvalFunction) {
        InterpretedCodeBlock* child = m_topCodeBlock->firstChild();
        while (child) {
            if (child->isFunctionDeclaration()) {
                if (child->lexicalBlockIndexFunctionLocatedIn() == 0 && !state.context()->globalObject()->defineOwnProperty(state, child->functionName(), ObjectPropertyDescriptor(Value(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)))) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::Code::SyntaxError, "Identifier '%s' has already been declared", child->functionName());
                }
            }
            child = child->nextSibling();
        }

        // https://www.ecma-international.org/ecma-262/#sec-globaldeclarationinstantiation
        IdentifierRecordVector* globalDeclarativeRecord = context()->globalDeclarativeRecord();
        size_t globalDeclarativeRecordLen = globalDeclarativeRecord->size();
        for (size_t i = 0; i < globalDeclarativeRecordLen; i++) {
            // For each name in varNames, do
            // If envRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
            for (size_t j = 0; j < identifierVectorLen; j++) {
                if (identifierVector[j].m_isVarDeclaration && identifierVector[j].m_name == globalDeclarativeRecord->at(i).m_name) {
                    ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, globalDeclarativeRecord->at(i).m_name.string(), false, String::emptyString, ErrorObject::Messages::DuplicatedIdentifier);
                }
            }
        }

        for (size_t i = 0; i < globalLexicalVectorLen; i++) {
            // Let hasRestrictedGlobal be ? envRec.HasRestrictedGlobalProperty(name).
            // If hasRestrictedGlobal is true, throw a SyntaxError exception.
            auto desc = context()->globalObject()->getOwnProperty(state, globalLexicalVector[i].m_name);
            if (desc.hasValue() && !desc.isConfigurable()) {
                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, globalLexicalVector[i].m_name.string(), false, String::emptyString, "redeclaration of non-configurable global property %s");
            }
        }
    }

    for (size_t i = 0; i < globalLexicalVectorLen; i++) {
        codeExecutionState->lexicalEnvironment()->record()->createBinding(*codeExecutionState, globalLexicalVector[i].m_name, false, globalLexicalVector[i].m_isMutable, false);
    }

    for (size_t i = 0; i < identifierVectorLen; i++) {
        // https://www.ecma-international.org/ecma-262/5.1/#sec-10.5
        // Step 2. If code is eval code, then let configurableBindings be true.
        if (identifierVector[i].m_isVarDeclaration) {
            globalVariableRecord->createBinding(*codeExecutionState, identifierVector[i].m_name, isExecuteOnEvalFunction, identifierVector[i].m_isMutable, true, m_topCodeBlock);
        }
    }

    Value thisValue(context()->globalObjectProxy());

    size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    Value* registerFile = (Value*)ALLOCA((byteCodeBlock->m_requiredRegisterFileSizeInValueSize + 1 + literalStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth()) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
    stackStorage[0] = thisValue;
    Value* literalStorage = stackStorage + 1 + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue = ByteCodeInterpreter::interpret(codeExecutionState, byteCodeBlock, 0, registerFile);
    clearStack<512>();

    // we give up program bytecodeblock after first excution for reducing memory usage
    m_topCodeBlock->m_byteCodeBlock = nullptr;

    return resultValue;
}

// NOTE: eval by direct call
Value Script::executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isStrictModeOutside, bool isEvalCodeOnFunction)
{
    ByteCodeBlock* byteCodeBlock = m_topCodeBlock->byteCodeBlock();

    EnvironmentRecord* record;
    bool inStrict = false;
    if (UNLIKELY(isStrictModeOutside)) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        inStrict = true;
        record = new DeclarativeEnvironmentRecordNotIndexed(state, true);
    } else {
        record = state.lexicalEnvironment()->record();
    }

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t vecLen = vec.size();

    // test there was let on block scope
    LexicalEnvironment* e = state.lexicalEnvironment();
    while (e) {
        if (e->record()->isDeclarativeEnvironmentRecord() && e->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
            break;
        }
        if (e->record()->isGlobalEnvironmentRecord()) {
            break;
        }

        // https://www.ecma-international.org/ecma-262/10.0/#sec-variablestatements-in-catch-blocks
        if (e->record()->isDeclarativeEnvironmentRecord() && e->record()->asDeclarativeEnvironmentRecord()->isDeclarativeEnvironmentRecordNotIndexed()) {
            if (e->record()->asDeclarativeEnvironmentRecord()->asDeclarativeEnvironmentRecordNotIndexed()->isCatchClause()) {
                e = e->outerEnvironment();
                continue;
            }
        }

        if (!m_topCodeBlock->isStrict()) {
            for (size_t i = 0; i < vecLen; i++) {
                if (vec[i].m_isVarDeclaration) {
                    auto slot = e->record()->hasBinding(state, vec[i].m_name);
                    if (slot.m_isLexicallyDeclared && slot.m_index != SIZE_MAX) {
                        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, vec[i].m_name.string(), false, String::emptyString, ErrorObject::Messages::DuplicatedIdentifier);
                    }
                }
            }
        }

        e = e->outerEnvironment();
    }

    EnvironmentRecord* recordToAddVariable = record;
    e = state.lexicalEnvironment();
    while (!recordToAddVariable->isVarDeclarationTarget()) {
        e = e->outerEnvironment();
        recordToAddVariable = e->record();
    }

    if (recordToAddVariable->isGlobalEnvironmentRecord()) {
        testDeclareGlobalFunctions(state, m_topCodeBlock, context()->globalObject());
    }

    for (size_t i = 0; i < vecLen; i++) {
        if (vec[i].m_isVarDeclaration) {
            recordToAddVariable->createBinding(state, vec[i].m_name, inStrict ? false : true, true, true, m_topCodeBlock);
        }
    }

    LexicalEnvironment* newEnvironment = new LexicalEnvironment(record, state.lexicalEnvironment());
    ExecutionState newState(&state, newEnvironment, m_topCodeBlock->isStrict());

    size_t stackStorageSize = m_topCodeBlock->totalStackAllocatedVariableSize();
    size_t identifierOnStackCount = m_topCodeBlock->identifierOnStackCount();
    size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    Value* registerFile = ALLOCA((byteCodeBlock->m_requiredRegisterFileSizeInValueSize + stackStorageSize + literalStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth()) * sizeof(Value), Value, state);
    registerFile[0] = Value();
    Value* stackStorage = registerFile + byteCodeBlock->m_requiredRegisterFileSizeInValueSize;
    for (size_t i = 0; i < identifierOnStackCount; i++) {
        stackStorage[i] = Value();
    }
    Value* literalStorage = stackStorage + stackStorageSize + m_topCodeBlock->lexicalBlockStackAllocatedIdentifierMaximumDepth();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    stackStorage[0] = thisValue;

    if (isEvalCodeOnFunction && m_topCodeBlock->usesArgumentsObject()) {
        AtomicString arguments = state.context()->staticStrings().arguments;

        FunctionEnvironmentRecord* fnRecord = nullptr;
        {
            LexicalEnvironment* env = state.lexicalEnvironment();
            while (env) {
                if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                    fnRecord = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                    break;
                }
                env = env->outerEnvironment();
            }
        }
        ASSERT(!!fnRecord);

        FunctionObject* callee = state.resolveCallee();
        if (fnRecord->hasBinding(newState, arguments).m_index == SIZE_MAX && callee->isScriptFunctionObject()) {
            // FIXME check if formal parameters does not contain a rest parameter, any binding patterns, or any initializers.
            bool isMapped = !callee->codeBlock()->hasParameterOtherThanIdentifier() && !inStrict;
            callee->asScriptFunctionObject()->generateArgumentsObject(newState, state.argc(), state.argv(), fnRecord, nullptr, isMapped);
        }
    }

    newState.ensureRareData()->m_codeBlock = m_topCodeBlock;
    Value resultValue = ByteCodeInterpreter::interpret(&newState, byteCodeBlock, 0, registerFile);
    clearStack<512>();

    return resultValue;
}
}
