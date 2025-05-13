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
#include "runtime/Global.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/JSON.h"
#include "runtime/SandBox.h"
#include "runtime/ScriptFunctionObject.h"
#include "runtime/ScriptAsyncFunctionObject.h"
#include "runtime/ModuleNamespaceObject.h"
#include "parser/ast/AST.h"

namespace Escargot {

void* Script::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(Script)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(Script, m_srcName));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(Script, m_sourceCode));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(Script, m_topCodeBlock));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(Script, m_moduleData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(Script));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

bool Script::isExecuted()
{
    if (isModule()) {
        return m_moduleData->m_status >= ModuleData::ModuleStatus::Evaluating;
    }
    return m_topCodeBlock->byteCodeBlock() == nullptr;
}

bool Script::wasThereErrorOnModuleEvaluation()
{
    return m_moduleData && m_moduleData->m_evaluationError.hasValue();
}

Value Script::moduleEvaluationError()
{
    if (m_moduleData && m_moduleData->m_evaluationError.hasValue()) {
        return m_moduleData->m_evaluationError.value();
    }
    return Value();
}

Context* Script::context()
{
    return m_topCodeBlock->context();
}

Script* Script::loadModuleFromScript(ExecutionState& state, ModuleRequest& request)
{
    Platform::LoadModuleResult result = Global::platform()->onLoadModule(context(), this, request.m_specifier, request.m_type);
    if (!result.script) {
        ErrorObject::throwBuiltinError(state, (ErrorCode)result.errorCode, result.errorMessage->toNonGCUTF8StringData().data());
        return nullptr;
    }
    if (!result.script->moduleData()->m_didCallLoadedCallback) {
        Global::platform()->didLoadModule(context(), this, result.script.value());
        result.script->moduleData()->m_didCallLoadedCallback = true;
    }
    return result.script.value();
}

size_t Script::moduleRequestsLength()
{
    if (!isModule()) {
        return 0;
    }
    return m_moduleData->m_requestedModules.size();
}

String* Script::moduleRequest(size_t i)
{
    ASSERT(isModule());
    return m_moduleData->m_requestedModules[i].m_specifier;
}

Value Script::moduleInstantiate(ExecutionState& state)
{
    ASSERT(isModule());
    if (!moduleData()->m_didCallLoadedCallback) {
        Global::platform()->didLoadModule(context(), nullptr, this);
        moduleData()->m_didCallLoadedCallback = true;
    }

    auto result = moduleLinking(state);
    if (result.gotException) {
        throw result.value;
    }
    return result.value;
}

Value Script::moduleEvaluate(ExecutionState& state)
{
    ASSERT(isModule());

    auto result = moduleEvaluation(state);
    if (result.gotException) {
        throw result.value;
    }
    return result.value;
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
        Script* requestedModule = loadModuleFromScript(state, e.m_moduleRequest.value());

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

Script::ResolveExportResult Script::resolveExport(ExecutionState& state, AtomicString exportName, std::vector<std::tuple<Script*, AtomicString>>& resolveSet)
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

    if (m_topCodeBlock == nullptr) {
        return Script::ResolveExportResult(Script::ResolveExportResult::Null);
    }

    // For each ExportEntry Record e in module.[[IndirectExportEntries]], do
    auto& indirectExportEntries = m_moduleData->m_indirectExportEntries;
    for (size_t i = 0; i < indirectExportEntries.size(); i++) {
        auto& e = indirectExportEntries[i];

        // If SameValue(exportName, e.[[ExportName]]) is true, then
        if (e.m_exportName == exportName) {
            // Let importedModule be ? HostResolveImportedModule(module, e.[[ModuleRequest]]).
            Script* importedModule = loadModuleFromScript(state, e.m_moduleRequest.value());

            // If e.[[ImportName]] is "*", then
            if (e.m_importName.value() == context()->staticStrings().asciiTable[(unsigned char)'*']) {
                // Assert: module does not provide the direct binding for this export.
                // Return ResolvedBinding Record { [[Module]]: importedModule, [[BindingName]]: "*namespace*" }.
                return Script::ResolveExportResult(Script::ResolveExportResult::Record, Optional<std::tuple<Script*, AtomicString>>(std::make_tuple(importedModule, context()->staticStrings().stringStarNamespaceStar)));
            } else {
                // Else,
                // Assert: module imports a specific binding for this export.
                // Return importedModule.ResolveExport(e.[[ImportName]], resolveSet).
                return importedModule->resolveExport(state, e.m_importName.value(), resolveSet);
            }
        }
    }

    // If SameValue(exportName, "default") is true, then
    if (exportName == context()->staticStrings().stringDefault) {
        // Assert: A default export was not explicitly defined by this module.
        // Throw a SyntaxError exception.
        // NOTE A default export cannot be provided by an export *.
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "The module '%s' does not provide an export named 'default'", srcName());
    }

    // Let starResolution be null.
    Script::ResolveExportResult starResolution(Script::ResolveExportResult::Null);

    // For each ExportEntry Record e in module.[[StarExportEntries]], do
    auto& starExportEntries = m_moduleData->m_starExportEntries;
    for (size_t i = 0; i < starExportEntries.size(); i++) {
        auto& e = starExportEntries[i];
        // Let importedModule be HostResolveImportedModule(module, e.[[ModuleRequest]]).
        Script* importedModule = loadModuleFromScript(state, e.m_moduleRequest.value());
        // Let resolution be importedModule.ResolveExport(exportName, resolveSet).
        auto resolution = importedModule->resolveExport(state, exportName, resolveSet);
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
    if (topCodeBlock->hasChildren()) {
        InterpretedCodeBlockVector& childrenVector = topCodeBlock->children();
        for (size_t i = 0; i < childrenVector.size(); i++) {
            auto c = childrenVector[i];
            if (c->isFunctionDeclaration() && c->lexicalBlockIndexFunctionLocatedIn() == 0) {
                if (!canDeclareGlobalFunction(state, globalObject, c->functionName())) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Identifier '%s' has already been declared", c->functionName());
                }
            }
        }
    }
}

Value Script::execute(ExecutionState& state, bool isExecuteOnEvalFunction, bool inStrictMode)
{
    if (UNLIKELY(isExecuted())) {
        if (!m_canExecuteAgain) {
            ESCARGOT_LOG_ERROR("You cannot re-execute this type of Script object");
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_topCodeBlock = state.context()->scriptParser().initializeScript(m_sourceCode, m_srcName, m_moduleData).script->m_topCodeBlock;
    }

    if (isModule()) {
        if (!moduleData()->m_didCallLoadedCallback) {
            Global::platform()->didLoadModule(context(), nullptr, this);
            moduleData()->m_didCallLoadedCallback = true;
        }

        // https://www.ecma-international.org/ecma-262/#sec-toplevelmoduleevaluationjob
        auto result = moduleLinking(state);
        if (result.gotException) {
            throw result.value;
        }
        result = moduleEvaluation(state);
        if (result.gotException) {
            throw result.value;
        }
        return result.value;
    }

    ByteCodeBlock* byteCodeBlock = m_topCodeBlock->byteCodeBlock();

    ExecutionState* newState;
    if (LIKELY(!m_topCodeBlock->isAsync())) {
        if (byteCodeBlock->needsExtendedExecutionState()) {
            newState = new (alloca(sizeof(ExtendedExecutionState))) ExtendedExecutionState(context());
        } else {
            newState = new (alloca(sizeof(ExecutionState))) ExecutionState(context());
        }
    } else {
        newState = new ExtendedExecutionState(context(), nullptr, nullptr, 0, nullptr, false);
    }
    ExecutionState* codeExecutionState = newState;

    EnvironmentRecord* globalRecord = new GlobalEnvironmentRecord(state, m_topCodeBlock, context()->globalObject(), context()->globalDeclarativeRecord(), context()->globalDeclarativeStorage());
    LexicalEnvironment* globalLexicalEnvironment = new LexicalEnvironment(globalRecord, nullptr);
    newState->setLexicalEnvironment(globalLexicalEnvironment, m_topCodeBlock->isStrict());

    EnvironmentRecord* globalVariableRecord = globalRecord;

    if (isExecuteOnEvalFunction) {
        // NOTE: ES5 10.4.2.1 eval in strict mode
        // + Indirect eval code creates a new declarative environment for lexically-scoped declarations (let)
        EnvironmentRecord* newVariableRecord = new DeclarativeEnvironmentRecordNotIndexed(state, true);
        ExecutionState* newVariableState = new ExtendedExecutionState(context());
        newVariableState->setLexicalEnvironment(new LexicalEnvironment(newVariableRecord, globalLexicalEnvironment), m_topCodeBlock->isStrict());
        newVariableState->setParent(newState);
        codeExecutionState = newVariableState;
        if (inStrictMode) {
            globalVariableRecord = newVariableRecord;
        }
    }

    testDeclareGlobalFunctions(state, m_topCodeBlock, context()->globalObject());

    const InterpretedCodeBlock::IdentifierInfoVector& identifierVector = m_topCodeBlock->identifierInfos();
    size_t identifierVectorLen = identifierVector.size();

    const auto& globalLexicalVector = m_topCodeBlock->blockInfo(0)->identifiers();
    size_t globalLexicalVectorLen = globalLexicalVector.size();

    if (!isExecuteOnEvalFunction) {
        if (m_topCodeBlock->hasChildren()) {
            InterpretedCodeBlockVector& childrenVector = m_topCodeBlock->children();
            for (size_t i = 0; i < childrenVector.size(); i++) {
                InterpretedCodeBlock* child = childrenVector[i];
                if (child->isFunctionDeclaration()) {
                    if (child->lexicalBlockIndexFunctionLocatedIn() == 0 && !state.context()->globalObject()->defineOwnProperty(state, child->functionName(), ObjectPropertyDescriptor(Value(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectStructurePropertyDescriptor::EnumerablePresent)))) {
                        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, "Identifier '%s' has already been declared", child->functionName());
                    }
                }
            }
        }

        // https://www.ecma-international.org/ecma-262/#sec-globaldeclarationinstantiation
        IdentifierRecordVector* globalDeclarativeRecord = context()->globalDeclarativeRecord();
        size_t globalDeclarativeRecordLen = globalDeclarativeRecord->size();
        for (size_t i = 0; i < globalDeclarativeRecordLen; i++) {
            // For each name in varNames, do
            // If envRec.HasLexicalDeclaration(name) is true, throw a SyntaxError
            for (size_t j = 0; j < identifierVectorLen; j++) {
                if (identifierVector[j].m_isVarDeclaration && identifierVector[j].m_name == globalDeclarativeRecord->at(i).m_name) {
                    ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, globalDeclarativeRecord->at(i).m_name.string(), false, String::emptyString(), ErrorObject::Messages::DuplicatedIdentifier);
                }
            }
        }

        for (size_t i = 0; i < globalLexicalVectorLen; i++) {
            // Let hasRestrictedGlobal be ? envRec.HasRestrictedGlobalProperty(name).
            // If hasRestrictedGlobal is true, throw a SyntaxError exception.
            auto desc = context()->globalObject()->getOwnProperty(state, globalLexicalVector[i].m_name);
            if (desc.hasValue() && !desc.isConfigurable()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, globalLexicalVector[i].m_name.string(), false, String::emptyString(), "redeclaration of non-configurable global property %s");
            }
        }
    }

    {
        VirtualIdDisabler d(context()); // we should create binding even there is virtual ID

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
    }

    Value thisValue(context()->globalObjectProxy());

    const size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    const size_t registerFileSize = byteCodeBlock->m_requiredTotalRegisterNumber;
    ASSERT(registerFileSize == byteCodeBlock->m_requiredOperandRegisterNumber + m_topCodeBlock->totalStackAllocatedVariableSize() + literalStorageSize);

    Value* registerFile;
    if (LIKELY(!m_topCodeBlock->isAsync())) {
        registerFile = ALLOCA(registerFileSize * sizeof(Value), Value);
    } else {
        registerFile = CustomAllocator<Value>().allocate(registerFileSize);
        // we need to reset allocated memory because customAllocator read it
        memset(static_cast<void*>(registerFile), 0, sizeof(Value) * registerFileSize);
    }
    registerFile[0] = Value();

    Value* stackStorage = registerFile + byteCodeBlock->m_requiredOperandRegisterNumber;
    stackStorage[0] = thisValue;

    Value* literalStorage = stackStorage + m_topCodeBlock->totalStackAllocatedVariableSize();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue;
    if (LIKELY(!m_topCodeBlock->isAsync())) {
#ifdef ESCARGOT_DEBUGGER
        // set the next(first) breakpoint to be stopped in a newer script execution
        context()->setAsAlwaysStopState();
#endif
        resultValue = Interpreter::interpret(codeExecutionState, byteCodeBlock, reinterpret_cast<size_t>(byteCodeBlock->m_code.data()), registerFile);
        clearStack<512>();

        // we give up program bytecodeblock after first excution for reducing memory usage
        m_topCodeBlock->setByteCodeBlock(nullptr);
    } else {
        ScriptAsyncFunctionObject* fakeFunctionObject = new ScriptAsyncFunctionObject(state, state.context()->globalObject()->asyncFunctionPrototype(), m_topCodeBlock, nullptr);
        auto ep = new ExecutionPauser(state, fakeFunctionObject, newState, registerFile, m_topCodeBlock->byteCodeBlock());
        newState->setPauseSource(ep);
        ep->m_promiseCapability = PromiseObject::newPromiseCapability(*newState, newState->context()->globalObject()->promise());
        resultValue = ExecutionPauser::start(*newState, newState->pauseSource().value(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::Async);
    }

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
                        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, vec[i].m_name.string(), false, String::emptyString(), ErrorObject::Messages::DuplicatedIdentifier);
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
    ExtendedExecutionState newState(&state, newEnvironment, m_topCodeBlock->isStrict());

    const size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    const size_t registerFileSize = byteCodeBlock->m_requiredTotalRegisterNumber;
    ASSERT(registerFileSize == byteCodeBlock->m_requiredOperandRegisterNumber + m_topCodeBlock->totalStackAllocatedVariableSize() + literalStorageSize);

    Value* registerFile = ALLOCA(registerFileSize * sizeof(Value), Value);
    registerFile[0] = Value();

    Value* stackStorage = registerFile + byteCodeBlock->m_requiredOperandRegisterNumber;
    stackStorage[0] = thisValue;

    Value* literalStorage = stackStorage + m_topCodeBlock->totalStackAllocatedVariableSize();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }


    if (isEvalCodeOnFunction && m_topCodeBlock->usesArgumentsObject()) {
        AtomicString arguments = state.context()->staticStrings().arguments;

        FunctionEnvironmentRecord* funcRecord = nullptr;
        ScriptFunctionObject* funcObject = nullptr;

        // find the most nearest lexical function which is not an arrow function
        ExecutionState* es = &state;
        while (es) {
            EnvironmentRecord* record = es->lexicalEnvironment()->record();
            if (record->isDeclarativeEnvironmentRecord() && record->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord() && !record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->isScriptArrowFunctionObject()) {
                funcRecord = record->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord();
                funcObject = funcRecord->functionObject()->asScriptFunctionObject();
                break;
            }
            es = es->parent();
        }
        ASSERT(!!funcRecord && !!funcObject && !funcObject->isScriptArrowFunctionObject());

        if (funcRecord->hasBinding(newState, arguments).m_index == SIZE_MAX) {
            // If eval code uses the arguments object but not yet created in outer function, generate it
            // FIXME check if formal parameters does not contain a rest parameter, any binding patterns, or any initializers.
            bool isMapped = !funcObject->interpretedCodeBlock()->hasParameterOtherThanIdentifier() && !inStrict;
            funcObject->generateArgumentsObject(newState, es->argc(), es->argv(), funcRecord, nullptr, isMapped);
        }
    }

    // add CodeBlock to be used in exception handling process
    newState.rareData()->m_codeBlock = m_topCodeBlock;
    Value resultValue = Interpreter::interpret(&newState, byteCodeBlock, reinterpret_cast<size_t>(byteCodeBlock->m_code.data()), registerFile);
    clearStack<512>();

    return resultValue;
}

void* Script::ModuleData::ModulePromiseObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(ModulePromiseObject)] = { 0 };
        PromiseObject::fillGCDescriptor(desc);
        GC_set_bit(desc, GC_WORD_OFFSET(ModulePromiseObject, m_referrer));
        GC_set_bit(desc, GC_WORD_OFFSET(ModulePromiseObject, m_loadedScript));
        GC_set_bit(desc, GC_WORD_OFFSET(ModulePromiseObject, m_value));
        descr = GC_make_descriptor(desc, GC_WORD_LEN(ModulePromiseObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

Script::ModuleExecutionResult Script::moduleLinking(ExecutionState& state)
{
    // On success, Instantiate transitions this module's [[Status]] from "unlinked" to "linked". On failure, an exception is thrown and this module's [[Status]] remains "unlinked".
    ASSERT(isModule());

    // Let module be this Cyclic Module Record.
    // Assert: module.[[Status]] is not "linking" or "evaluating".
    ASSERT(moduleData()->m_status != ModuleData::Linking && moduleData()->m_status != ModuleData::Evaluating);
    // Let stack be a new empty List.
    std::vector<Script*> stack;
    // Let result be InnerModuleInstantiation(module, stack, 0).
    ModuleExecutionResult result = innerModuleLinking(state, stack, 0);
    // If result is an abrupt completion, then
    if (result.gotException) {
        // For each module m in stack, do
        for (size_t i = 0; i < stack.size(); i++) {
            // Assert: m.[[Status]] is "linking".
            ASSERT(stack[i]->isModule());
            ModuleData* m = stack[i]->moduleData();
            ASSERT(m->m_status == ModuleData::Linking);
            // Set m.[[Status]] to "unlinked".
            m->m_status = ModuleData::Unlinked;
            // Set m.[[Environment]] to undefined.
            m->m_moduleRecord = nullptr;
            // Set m.[[DFSIndex]] to undefined.
            m->m_dfsIndex.reset();
            // Set m.[[DFSAncestorIndex]] to undefined.
            m->m_dfsAncestorIndex.reset();
        }
        // Assert: module.[[Status]] is "unlinked".
        ASSERT(moduleData()->m_status == ModuleData::Unlinked);
        // Return result.
        return result;
    }
    // Assert: module.[[Status]] is "linked" or "evaluated".
    ASSERT(moduleData()->m_status == ModuleData::Linked || moduleData()->m_status == ModuleData::Evaluated);
    // Assert: stack is empty.
    ASSERT(stack.empty());
    // Return undefined.
    return ModuleExecutionResult(false, Value());
}

Script::ModuleExecutionResult Script::innerModuleLinking(ExecutionState& state, std::vector<Script*>& stack, uint32_t index)
{
    // If module is not a Cyclic Module Record, then
    //    Perform ? module.Instantiate().
    //    Return index.
    ASSERT(isModule());

    // If module.[[Status]] is "linking", "linked", or "evaluated", then
    ModuleData* md = moduleData();
    if (md->m_status == ModuleData::Linking || md->m_status == ModuleData::Linked || md->m_status == ModuleData::Evaluated) {
        // Return index.
        return Script::ModuleExecutionResult(false, Value(index));
    }

    // Assert: module.[[Status]] is "unlinked".
    ASSERT(md->m_status == ModuleData::Unlinked);
    // Set module.[[Status]] to "linking".
    md->m_status = ModuleData::Linking;
    // Set module.[[DFSIndex]] to index.
    md->m_dfsIndex = index;
    // Set module.[[DFSAncestorIndex]] to index.
    md->m_dfsAncestorIndex = index;
    // Increase index by 1.
    index++;
    // Append module to stack.
    stack.push_back(this);

    // For each String required that is an element of module.[[RequestedModules]], do
    size_t rmLength = moduleRequestsLength();
    for (size_t i = 0; i < rmLength; i++) {
        // Let requiredModule be ! HostResolveImportedModule(module, required).
        Script* requiredModule = loadModuleFromScript(state, m_moduleData->m_requestedModules[i]);
        // NOTE: Instantiate must be completed successfully prior to invoking this method, so every requested module is guaranteed to resolve successfully.
        // Set index to ? innerModuleInstantiation(requiredModule, stack, index).
        auto result = requiredModule->innerModuleLinking(state, stack, index);
        if (result.gotException) {
            return result;
        }
        index = result.value.asNumber();

        // Assert: requiredModule.[[Status]] is either "linking", "linked", or "evaluated".
        ASSERT(requiredModule->moduleData()->m_status == ModuleData::Linking || requiredModule->moduleData()->m_status == ModuleData::Linked || requiredModule->moduleData()->m_status == ModuleData::Evaluated);
        // Assert: requiredModule.[[Status]] is "linking" if and only if requiredModule is in stack.
        // this assert is removed. because some users want to instantiate their module on onLoadModule

        // If requiredModule.[[Status]] is "linking", then
        if (requiredModule->moduleData()->m_status == ModuleData::Linking) {
            // Set module.[[DFSAncestorIndex]] to min(module.[[DFSAncestorIndex]], requiredModule.[[DFSAncestorIndex]]).
            md->m_dfsAncestorIndex = std::min(md->m_dfsAncestorIndex.value(), requiredModule->moduleData()->m_dfsAncestorIndex.value());
        }
    }

    // Perform ? module.InitializeEnvironment().
    auto result = moduleInitializeEnvironment(state);
    if (!result.isEmpty()) {
        return Script::ModuleExecutionResult(true, result);
    }

    // Assert: module occurs exactly once in stack.
    // Assert: module.[[DFSAncestorIndex]] is less than or equal to module.[[DFSIndex]].
    ASSERT(md->m_dfsAncestorIndex.value() <= md->m_dfsIndex.value());

    // If module.[[DFSAncestorIndex]] equals module.[[DFSIndex]], then
    if (md->m_dfsAncestorIndex.value() == md->m_dfsIndex.value()) {
        // Let done be false.
        bool done = false;
        // Repeat, while done is false,
        while (!done) {
            // Let requiredModule be the last element in stack.
            Script* requiredModule = stack.back();
            // Remove the last element of stack.
            stack.pop_back();
            // Set requiredModule.[[Status]] to "linked".
            requiredModule->moduleData()->m_status = ModuleData::Linked;
            // If requiredModule and module are the same Module Record, set done to true.
            if (requiredModule == this) {
                done = true;
            }
        }
    }
    // Return index.
    return Script::ModuleExecutionResult(false, Value(index));
}

Script::ModuleExecutionResult Script::moduleEvaluation(ExecutionState& state)
{
    // + https://tc39.es/proposal-top-level-await/#sec-moduleevaluation

    // Let module be this Cyclic Module Record.
    ModuleData* md = moduleData();
    // Assert: module.[[Status]] is "linked" or "evaluated".
    ASSERT(md->m_status == ModuleData::Linked || md->m_status == ModuleData::Evaluated);

    // If module.[[Status]] is evaluated, set module to module.[[CycleRoot]].
    if (md->m_status == ModuleData::Evaluated) {
        md->m_cycleRoot = this;
    }
    // If module.[[TopLevelCapability]] is not empty, then
    if (md->m_topLevelCapability.hasValue()) {
        // Return module.[[TopLevelCapability]].[[Promise]].
        return Script::ModuleExecutionResult(false, md->m_topLevelCapability.value().m_promise);
    }

    // Let stack be a new empty List.
    std::vector<Script*> stack;

    // Let capability be ! NewPromiseCapability(%Promise%).
    auto capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // Set module.[[TopLevelCapability]] to capability.
    md->m_topLevelCapability = capability;

    // Let result be InnerModuleEvaluation(module, stack, 0).
    auto result = innerModuleEvaluation(state, stack, 0);
    // If result is an abrupt completion, then
    if (result.gotException) {
        // For each module m in stack, do
        for (size_t i = 0; i < stack.size(); i++) {
            // Assert: m.[[Status]] is "evaluating".
            ASSERT(stack[i]->moduleData()->m_status == ModuleData::Evaluating);
            // Set m.[[Status]] to "evaluated".
            // Set m.[[EvaluationError]] to result.
            // Assert: module.[[Status]] is "evaluated" and module.[[EvaluationError]] is result.
            stack[i]->moduleData()->m_status = ModuleData::Evaluated;
            stack[i]->moduleData()->m_evaluationError = EncodedValue(result.value);
        }
        // Perform ! Call(capability.[[Reject]], undefined, « result.[[Value]] »).
        Value arg = result.value;
        Object::call(state, capability.m_rejectFunction, Value(), 1, &arg);
    } else {
        // Otherwise,
        // Assert: module.[[Status]] is "evaluated" and module.[[EvaluationError]] is undefined.
        // If module.[[AsyncEvaluating]] is false, then
        if (!md->m_asyncEvaluating) {
            // Perform ! Call(capability.[[Resolve]], undefined, « undefined »).
            Value arg;
            Object::call(state, capability.m_resolveFunction, Value(), 1, &arg);
        }
        // Assert: stack is empty.
    }

    // Return capability.[[Promise]].
    // FIXME return promise
    return result;
}

Script::ModuleExecutionResult Script::innerModuleEvaluation(ExecutionState& state, std::vector<Script*>& stack, uint32_t index)
{
    //+ https://tc39.es/proposal-top-level-await/#sec-innermoduleevaluation
    // If module is not a Cyclic Module Record, then
    //     Perform ? module.Evaluate().
    //     Return index.
    ASSERT(isModule());

    ModuleData* md = moduleData();

    // If module.[[Status]] is "evaluated", then
    if (md->m_status == ModuleData::Evaluated) {
        // If module.[[EvaluationError]] is undefined, return index.
        if (!md->m_evaluationError.hasValue() || Value(md->m_evaluationError.value()).isUndefined()) {
            return Script::ModuleExecutionResult(false, Value(index));
        }
        // Otherwise return module.[[EvaluationError]].
        return Script::ModuleExecutionResult(true, md->m_evaluationError.value());
    }

    // If module.[[Status]] is "evaluating", return index.
    if (md->m_status == ModuleData::Evaluating) {
        return Script::ModuleExecutionResult(false, Value(index));
    }

    // Assert: module.[[Status]] is "linked".
    ASSERT(md->m_status == ModuleData::Linked);
    // Set module.[[Status]] to "evaluating".
    md->m_status = ModuleData::Evaluating;
    // Set module.[[DFSIndex]] to index.
    md->m_dfsIndex = index;
    // Set module.[[DFSAncestorIndex]] to index.
    md->m_dfsAncestorIndex = index;
    // Set module.[[PendingAsyncDependencies]] to 0.
    md->m_pendingAsyncDependencies = size_t(0);
    // Increase index by 1.
    index++;
    // Append module to stack.
    stack.push_back(this);

    // For each String required that is an element of module.[[RequestedModules]], do
    size_t rmLength = moduleRequestsLength();
    for (size_t i = 0; i < rmLength; i++) {
        // Let requiredModule be ! HostResolveImportedModule(module, required).
        Script* requiredModule = loadModuleFromScript(state, m_moduleData->m_requestedModules[i]);
        // NOTE: Instantiate must be completed successfully prior to invoking this method, so every requested module is guaranteed to resolve successfully.
        // Set index to ? InnerModuleEvaluation(requiredModule, stack, index).
        auto result = requiredModule->innerModuleEvaluation(state, stack, index);
        if (result.gotException) {
            return result;
        }
        index = result.value.asNumber();
        // Assert: requiredModule.[[Status]] is either "evaluating" or "evaluated".
        ASSERT(requiredModule->moduleData()->m_status == ModuleData::Evaluating || requiredModule->moduleData()->m_status == ModuleData::Evaluated);
// Assert: requiredModule.[[Status]] is "evaluating" if and only if requiredModule is in stack.
#if !defined(NDEBUG)
        if (requiredModule->moduleData()->m_status == ModuleData::Evaluating) {
            bool onStack = false;
            for (size_t j = 0; j < stack.size(); j++) {
                if (stack[j] == requiredModule) {
                    onStack = true;
                    break;
                }
            }
            ASSERT(onStack);
        }
#endif
        // If requiredModule.[[Status]] is "evaluating", then
        if (requiredModule->moduleData()->m_status == ModuleData::Evaluating) {
            // Assert: requiredModule is a Cyclic Module Record.
            ASSERT(requiredModule->isModule());
            // Set module.[[DFSAncestorIndex]] to min(module.[[DFSAncestorIndex]], requiredModule.[[DFSAncestorIndex]]).
            md->m_dfsAncestorIndex = std::min(md->m_dfsAncestorIndex.value(), requiredModule->moduleData()->m_dfsAncestorIndex.value());
        } else {
            // Otherwise,
            // Set requiredModule to requiredModule.[[CycleRoot]].
            requiredModule = requiredModule->moduleData()->m_cycleRoot.value();
            // Assert: requiredModule.[[Status]] is evaluated.
            ASSERT(requiredModule->moduleData()->m_status == ModuleData::Evaluated);
            // If requiredModule.[[EvaluationError]] is not empty, return requiredModule.[[EvaluationError]].
            if (requiredModule->moduleData()->m_evaluationError.hasValue()) {
                return Script::ModuleExecutionResult(true, requiredModule->moduleData()->m_evaluationError.value());
            }
        }

        // If requiredModule.[[AsyncEvaluating]] is true, then
        if (requiredModule->moduleData()->m_asyncEvaluating) {
            // Set module.[[PendingAsyncDependencies]] to module.[[PendingAsyncDependencies]] + 1.
            md->m_pendingAsyncDependencies = size_t(md->m_pendingAsyncDependencies.value() + 1);
            // Append module to requiredModule.[[AsyncParentModules]].
            requiredModule->moduleData()->m_asyncParentModules.pushBack(this);
        }
    }

    if (m_topCodeBlock == nullptr) {
        // Synthetic module evaluation
        ModuleEnvironmentRecord* moduleRecord = md->m_moduleRecord;
        moduleRecord->createBinding(state, state.context()->staticStrings().stringStarDefaultStar, false, false, false);

        try {
            moduleRecord->initializeBinding(state, state.context()->staticStrings().stringStarDefaultStar, JSON::parse(state, sourceCode(), Value()));
        } catch (const Value& e) {
            md->m_evaluationError = EncodedValue(e);
        }

    } else if (md->m_pendingAsyncDependencies.hasValue() && md->m_pendingAsyncDependencies.value() > 0) {
        // If module.[[PendingAsyncDependencies]] > 0, set module.[[AsyncEvaluating]] to true.
        md->m_asyncEvaluating = true;
    } else if (m_topCodeBlock->isAsync()) {
        // Otherwise, if module.[[Async]] is true, perform ! ExecuteAsyncModule(module).
        moduleExecuteAsyncModule(state);
    } else {
        // Otherwise, perform ? module.ExecuteModule().
        auto result = moduleExecute(state);
        if (result.gotException) {
            return result;
        }
    }

    // Assert: module occurs exactly once in stack.
    // Assert: module.[[DFSAncestorIndex]] is less than or equal to module.[[DFSIndex]].
    ASSERT(md->m_dfsAncestorIndex.value() <= md->m_dfsIndex.value());

    // If module.[[DFSAncestorIndex]] equals module.[[DFSIndex]], then
    if (md->m_dfsAncestorIndex.value() == md->m_dfsIndex.value()) {
        // Let cycleRoot be module.
        auto cycleRoot = this;
        // Let done be false.
        bool done = false;
        // Repeat, while done is false,
        while (!done) {
            // Let requiredModule be the last element in stack.
            Script* requiredModule = stack.back();
            // Remove the last element of stack.
            stack.pop_back();
            // Set requiredModule.[[Status]] to "evaluated".
            requiredModule->moduleData()->m_status = ModuleData::Evaluated;
            // If requiredModule and module are the same Module Record, set done to true.
            if (requiredModule == this) {
                done = true;
            }
            // Set requiredModule.[[CycleRoot]] to cycleRoot.
            requiredModule->moduleData()->m_cycleRoot = cycleRoot;
        }
    }
    // Return index.
    return Script::ModuleExecutionResult(false, Value(index));
}

Value Script::moduleInitializeEnvironment(ExecutionState& state)
{
    ASSERT(m_moduleData->m_moduleRecord == nullptr);

    // For each ExportEntry Record e in module.[[IndirectExportEntries]], do
    auto& indirectExportEntries = m_moduleData->m_indirectExportEntries;
    for (size_t i = 0; i < indirectExportEntries.size(); i++) {
        auto& e = indirectExportEntries[i];
        // Let resolution be ? module.ResolveExport(e.[[ExportName]], « »).
        auto resolution = resolveExport(state, e.m_exportName.value());
        // If resolution is null or "ambiguous", throw a SyntaxError exception.
        if (resolution.m_type == ResolveExportResult::Null || resolution.m_type == ResolveExportResult::Ambiguous) {
            StringBuilder builder;
            builder.appendString("The export binding '");
            builder.appendString(e.m_exportName.value().string());
            builder.appendString("' is ambiguous");
            return ErrorObject::createBuiltinError(state, ErrorCode::SyntaxError, builder.finalize(&state)->toNonGCUTF8StringData().data());
        }
        // Assert: resolution is a ResolvedBinding Record.
    }

    ModuleEnvironmentRecord* moduleRecord = new ModuleEnvironmentRecord(this);
    m_moduleData->m_moduleRecord = moduleRecord;

    const InterpretedCodeBlock::IdentifierInfoVector& vec = m_topCodeBlock->identifierInfos();
    size_t len = vec.size();
    for (size_t i = 0; i < len; i++) {
        moduleRecord->createBinding(state, vec[i].m_name, false, vec[i].m_isMutable, true);
    }

    InterpretedCodeBlock::BlockInfo* bi = m_topCodeBlock->blockInfo(0);
    if (bi) {
        len = bi->identifiers().size();
        for (size_t i = 0; i < len; i++) {
            moduleRecord->createBinding(state, bi->identifiers()[i].m_name, false, bi->identifiers()[i].m_isMutable, false);
        }
    }

    // For each ImportEntry Record in in module.[[ImportEntries]], do
    for (size_t i = 0; i < m_moduleData->m_importEntries.size(); i++) {
        auto& in = m_moduleData->m_importEntries[i];
        // Let importedModule be ! HostResolveImportedModule(module, in.[[ModuleRequest]]).
        Script* importedModule = loadModuleFromScript(state, in.m_moduleRequest);
        // NOTE: The above call cannot fail because imported module requests are a subset of module.[[RequestedModules]], and these have been resolved earlier in this algorithm.

        // If in.[[ImportName]] is "*", then
        if (in.m_importName == context()->staticStrings().asciiTable[(unsigned char)'*']) {
            // Let namespace be ? GetModuleNamespace(importedModule).
            ModuleNamespaceObject* namespaceObject = importedModule->getModuleNamespace(state);
            // Perform ! envRec.CreateImmutableBinding(in.[[LocalName]], true).
            // Call envRec.InitializeBinding(in.[[LocalName]], namespace).
            moduleRecord->initializeBinding(state, in.m_localName, namespaceObject);
        } else {
            // Let resolution be importedModule.ResolveExport(in.[[ImportName]], « », «‍ »).
            auto resolution = importedModule->resolveExport(state, in.m_importName);
            // ReturnIfAbrupt(resolution).
            // If resolution is null or resolution is "ambiguous", throw a SyntaxError exception.
            if (resolution.m_type == Script::ResolveExportResult::Null) {
                StringBuilder builder;
                builder.appendString("The requested module '");
                builder.appendString(in.m_moduleRequest.m_specifier);
                builder.appendString("' does not provide an export named '");
                builder.appendString(in.m_localName.string());
                builder.appendString("'");
                return ErrorObject::createBuiltinError(state, ErrorCode::SyntaxError, builder.finalize(&state)->toNonGCUTF8StringData().data());
            } else if (resolution.m_type == Script::ResolveExportResult::Ambiguous) {
                StringBuilder builder;
                builder.appendString("The requested module '");
                builder.appendString(in.m_moduleRequest.m_specifier);
                builder.appendString("' does not provide an export named '");
                builder.appendString(in.m_localName.string());
                builder.appendString("' correctly");
                return ErrorObject::createBuiltinError(state, ErrorCode::SyntaxError, builder.finalize(&state)->toNonGCUTF8StringData().data());
            }

            // If resolution.[[BindingName]] is "*namespace*", then
            if (std::get<1>(resolution.m_record.value()) == context()->staticStrings().stringStarNamespaceStar) {
                // Let namespace be ? GetModuleNamespace(resolution.[[Module]]).
                auto namespaceObject = std::get<0>(resolution.m_record.value())->getModuleNamespace(state);
                // Perform ! env.CreateImmutableBinding(in.[[LocalName]], true).
                // Call env.InitializeBinding(in.[[LocalName]], namespace).
                moduleRecord->initializeBinding(state, in.m_localName, namespaceObject);
            } else {
                ASSERT(std::get<0>(resolution.m_record.value())->moduleData()->m_moduleRecord != nullptr);
                // Call envRec.CreateImportBinding(in.[[LocalName]], resolution.[[module]], resolution.[[bindingName]]).
                moduleRecord->createImportBinding(state, in.m_localName, std::get<0>(resolution.m_record.value())->moduleData()->m_moduleRecord, std::get<1>(resolution.m_record.value()));
            }
        }
    }

    return Value(Value::EmptyValue);
}

Script::ModuleExecutionResult Script::moduleExecute(ExecutionState& state, Optional<PromiseReaction::Capability> capability)
{
    ASSERT(isModule());

    ByteCodeBlock* byteCodeBlock = m_topCodeBlock->byteCodeBlock();

    LexicalEnvironment* globalLexicalEnv = new LexicalEnvironment(
        new GlobalEnvironmentRecord(state, m_topCodeBlock, context()->globalObject(), context()->globalDeclarativeRecord(), context()->globalDeclarativeStorage()), nullptr);

    ExecutionState* newState;
    if (LIKELY(!m_topCodeBlock->isAsync())) {
        if (byteCodeBlock->needsExtendedExecutionState()) {
            newState = new (alloca(sizeof(ExtendedExecutionState))) ExtendedExecutionState(context());
        } else {
            newState = new (alloca(sizeof(ExecutionState))) ExecutionState(context());
        }
    } else {
        newState = new ExtendedExecutionState(context(), nullptr, nullptr, 0, nullptr, false);
    }
    newState->setLexicalEnvironment(new LexicalEnvironment(moduleData()->m_moduleRecord, globalLexicalEnv), true);

    const size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    const size_t registerFileSize = byteCodeBlock->m_requiredTotalRegisterNumber;
    ASSERT(registerFileSize == byteCodeBlock->m_requiredOperandRegisterNumber + m_topCodeBlock->totalStackAllocatedVariableSize() + literalStorageSize);

    Value* registerFile;
    if (LIKELY(!m_topCodeBlock->isAsync())) {
        registerFile = ALLOCA(registerFileSize * sizeof(Value), Value);
    } else {
        registerFile = CustomAllocator<Value>().allocate(registerFileSize);
        // we need to reset allocated memory because customAllocator read it
        memset(static_cast<void*>(registerFile), 0, sizeof(Value) * registerFileSize);
    }

    registerFile[0] = Value();
    Value* stackStorage = registerFile + byteCodeBlock->m_requiredOperandRegisterNumber;
    stackStorage[0] = Value();
    Value* literalStorage = stackStorage + m_topCodeBlock->totalStackAllocatedVariableSize();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue;
    bool gotException = false;

    if (LIKELY(!m_topCodeBlock->isAsync())) {
        try {
            Interpreter::interpret(newState, byteCodeBlock, reinterpret_cast<size_t>(byteCodeBlock->m_code.data()), registerFile);
        } catch (const Value& e) {
            resultValue = e;
            gotException = true;
        }

        clearStack<512>();

        // we give up program bytecodeblock after first excution for reducing memory usage
        m_topCodeBlock->setByteCodeBlock(nullptr);
    } else {
        ASSERT(capability);
        ScriptAsyncFunctionObject* fakeFunctionObject = new ScriptAsyncFunctionObject(state, state.context()->globalObject()->asyncFunctionPrototype(), m_topCodeBlock, nullptr);
        auto ep = new ExecutionPauser(state, fakeFunctionObject, newState, registerFile, m_topCodeBlock->byteCodeBlock());
        newState->setPauseSource(ep);
        ep->m_promiseCapability = capability.value();
        resultValue = ExecutionPauser::start(*newState, newState->pauseSource().value(), newState->pauseSource()->sourceObject(), Value(), false, false, ExecutionPauser::StartFrom::Async);
    }

    return ModuleExecutionResult(gotException, resultValue);
}

// https://tc39.es/proposal-top-level-await/#sec-async-module-execution-fulfilled
void Script::asyncModuleFulfilled(ExecutionState& state, Script* module)
{
    // Assert: module.[[Status]] is evaluated.
    ASSERT(module->moduleData()->m_status == Script::ModuleData::Evaluated);
    // If module.[[AsyncEvaluating]] is false,
    if (!module->moduleData()->m_asyncEvaluating) {
        // Assert: module.[[EvaluationError]] is not empty.
        ASSERT(module->moduleData()->m_evaluationError.hasValue());
        // Return undefined.
        return;
    }
    // Assert: module.[[EvaluationError]] is empty.
    ASSERT(!module->moduleData()->m_evaluationError.hasValue());
    // Set module.[[AsyncEvaluating]] to false.
    module->moduleData()->m_asyncEvaluating = false;

    // For each Module m of module.[[AsyncParentModules]], do
    for (size_t i = 0; i < module->moduleData()->m_asyncParentModules.size(); i++) {
        auto m = module->moduleData()->m_asyncParentModules[i];
        // Decrement m.[[PendingAsyncDependencies]] by 1.
        m->moduleData()->m_pendingAsyncDependencies = m->moduleData()->m_pendingAsyncDependencies.value() - 1;
        // If m.[[PendingAsyncDependencies]] is 0 and m.[[EvaluationError]] is empty, then
        if (m->moduleData()->m_pendingAsyncDependencies && !m->moduleData()->m_evaluationError.hasValue()) {
            // Assert: m.[[AsyncEvaluating]] is true.
            ASSERT(m->moduleData()->m_asyncEvaluating);
            // If m.[[CycleRoot]].[[EvaluationError]] is not empty, return undefined.
            if (m->moduleData()->m_cycleRoot->moduleData()->m_evaluationError.hasValue()) {
                return;
            }
            // If m.[[Async]] is true, then
            if (m->topCodeBlock()->isAsync()) {
                // Perform ! ExecuteAsyncModule(m).
                m->moduleExecuteAsyncModule(state);
            } else {
                // Otherwise,
                // Let result be m.ExecuteModule().
                auto result = m->moduleExecute(state);
                // If result is a normal completion,
                if (!result.gotException) {
                    // Perform ! AsyncModuleExecutionFulfilled(m).
                    asyncModuleFulfilled(state, m);
                } else {
                    // Otherwise,
                    // Perform ! AsyncModuleExecutionRejected(m, result.[[Value]]).
                    asyncModuleRejected(state, m, result.value);
                }
            }
        }
    }

    for (size_t i = 0; i < module->moduleData()->m_asyncPendingPromises.size(); i++) {
        module->moduleData()->m_asyncPendingPromises[i]->fulfill(state, module->getModuleNamespace(state));
    }
    module->moduleData()->m_asyncPendingPromises.clear();
}

Value Script::asyncModuleFulfilledFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    Script* module = self->internalSlotAsPointer<Script>(0);
    Script::asyncModuleFulfilled(state, module);
    return Value();
}

// https://tc39.es/proposal-top-level-await/#sec-async-module-execution-rejected
void Script::asyncModuleRejected(ExecutionState& state, Script* module, Value error)
{
    // Assert: module.[[Status]] is evaluated.
    ASSERT(module->moduleData()->m_status == Script::ModuleData::Evaluated);
    // If module.[[AsyncEvaluating]] is false,
    if (!module->moduleData()->m_asyncEvaluating) {
        // Assert: module.[[EvaluationError]] is not empty.
        ASSERT(module->moduleData()->m_evaluationError.hasValue());
        // Return undefined.
        return;
    }

    // Assert: module.[[EvaluationError]] is empty.
    ASSERT(!module->moduleData()->m_evaluationError.hasValue());
    // Set module.[[EvaluationError]] to ThrowCompletion(error).
    module->moduleData()->m_evaluationError = EncodedValue(error);
    // Set module.[[AsyncEvaluating]] to false.
    module->moduleData()->m_asyncEvaluating = false;
    // For each Module m of module.[[AsyncParentModules]], do
    for (size_t i = 0; i < module->moduleData()->m_asyncParentModules.size(); i++) {
        // Perform ! AsyncModuleExecutionRejected(m, error).
        asyncModuleRejected(state, module->moduleData()->m_asyncParentModules[i], error);
    }
    // If module.[[TopLevelCapability]] is not empty, then
    if (module->moduleData()->m_topLevelCapability) {
        // Assert: module.[[CycleRoot]] is equal to module.
        ASSERT(module->moduleData()->m_cycleRoot.value() == module);
        // Perform ! Call(module.[[TopLevelCapability]].[[Reject]], undefined, «error»).
        Value argv = error;
        Object::call(state, module->moduleData()->m_topLevelCapability.value().m_rejectFunction, Value(), 1, &argv);
    }

    // Return undefined.
    // throw error instead of return undefined
    // if don't throw error, user cannot notify the error was occured
    if (module->moduleData()->m_asyncPendingPromises.size()) {
        for (size_t i = 0; i < module->moduleData()->m_asyncPendingPromises.size(); i++) {
            module->moduleData()->m_asyncPendingPromises[i]->reject(state, error);
        }
        module->moduleData()->m_asyncPendingPromises.clear();
    } else {
        state.throwException(error);
    }
}

Value Script::asyncModuleRejectedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    Script* module = self->internalSlotAsPointer<Script>(0);
    Script::asyncModuleRejected(state, module, argv[0]);
    return Value();
}

void Script::moduleExecuteAsyncModule(ExecutionState& state)
{
    auto md = moduleData();
    // Assert: module.[[Status]] is evaluating or evaluated.
    ASSERT(md->m_status == ModuleData::Evaluating || md->m_status == ModuleData::Evaluated);
    // Assert: module.[[Async]] is true.
    ASSERT(m_topCodeBlock->isAsync());
    // Set module.[[AsyncEvaluating]] to true.
    md->m_asyncEvaluating = true;
    // Let capability be ! NewPromiseCapability(%Promise%).
    auto capability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // Let stepsFulfilled be the steps of a CallAsyncModuleFulfilled function as specified below.
    // Let onFulfilled be CreateBuiltinFunction(stepsFulfilled, « [[Module]] »).
    // Set onFulfilled.[[Module]] to module.
    // Let stepsRejected be the steps of a CallAsyncModuleRejected function as specified below.
    // Let onRejected be CreateBuiltinFunction(stepsRejected, « [[Module]] »).
    // Set onRejected.[[Module]] to module.
    ExtendedNativeFunctionObject* onFulfilled = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), asyncModuleFulfilledFunction, 1));
    onFulfilled->setInternalSlotAsPointer(0, this);
    ExtendedNativeFunctionObject* onRejected = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), asyncModuleRejectedFunction, 1));
    onRejected->setInternalSlotAsPointer(0, this);
    // Perform ! PerformPromiseThen(capability.[[Promise]], onFulfilled, onRejected).
    capability.m_promise->asPromiseObject()->then(state, onFulfilled, onRejected);

    // Perform ! module.ExecuteModule(capability).
    moduleExecute(state, capability);
    // Return.
}

ModuleNamespaceObject* Script::getModuleNamespace(ExecutionState& state)
{
    // Assert: module is an instance of a concrete subclass of Module Record.
    ASSERT(isModule());
    // Assert: module.[[Status]] is not "unlinked".
    ASSERT(moduleData()->m_status != ModuleData::Unlinked);

    if (!moduleData()->m_namespace) {
        moduleData()->m_namespace = new ModuleNamespaceObject(state, this);
    }

    return moduleData()->m_namespace.value();
}

Object* Script::importMetaProperty(ExecutionState& state)
{
    ASSERT(isModule());
    // Let importMeta be module.[[ImportMeta]].
    // If importMeta is empty, then
    if (!moduleData()->m_importMeta) {
        // Set importMeta to ! OrdinaryObjectCreate(null).
        Object* importMeta = new Object(state, Object::PrototypeIsNull);
        // Let importMetaValues be ! HostGetImportMetaProperties(module).
        // For each Record { [[Key]], [[Value]] } p that is an element of importMetaValues, do
        //     Perform ! CreateDataPropertyOrThrow(importMeta, p.[[Key]], p.[[Value]]).
        // Perform ! HostFinalizeImportMeta(importMeta, module).
        importMeta->defineOwnProperty(state, state.context()->staticStrings().lazyURL(),
                                      ObjectPropertyDescriptor(Value(srcName()), ObjectPropertyDescriptor::AllPresent));

        // Set module.[[ImportMeta]] to importMeta.
        moduleData()->m_importMeta = importMeta;
        // Return importMeta.
        return importMeta;
    } else {
        // Else,
        // Assert: Type(importMeta) is Object.
        // Return importMeta.
        return moduleData()->m_importMeta.value();
    }
}
} // namespace Escargot
