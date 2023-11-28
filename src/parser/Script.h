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

#ifndef __EscargotScript__
#define __EscargotScript__

#include "runtime/PromiseObject.h"
#include "runtime/Platform.h"

namespace Escargot {

class InterpretedCodeBlock;
class Context;
class ModuleEnvironmentRecord;
class ModuleNamespaceObject;

class Script : public gc {
    friend class ScriptParser;
    friend class GlobalObject;
    friend class ModuleNamespaceObject;

public:
    struct ModuleRequest {
        // Specifier (name) of the module (usually a file system entry)
        String* m_specifier;
        // Type of the module (cached value of [[Assertions]] where items.[[Key]] is "type")
        Platform::ModuleType m_type;

        ModuleRequest()
            : m_specifier(nullptr)
            , m_type(Platform::ModuleES)
        {
        }

        ModuleRequest(String* specifier, Platform::ModuleType type)
            : m_specifier(specifier)
            , m_type(type)
        {
        }
    };
    typedef Vector<ModuleRequest, GCUtil::gc_malloc_allocator<ModuleRequest>> ModuleRequestVector;

    // https://tc39.es/ecma262/#importentry-record
    struct ImportEntry {
        // [[ModuleRequest]]  ModuleRequest  The value of the ModuleSpecifier and its assertions of the ImportDeclaration.
        ModuleRequest m_moduleRequest;
        // [[ImportName]]  String  The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. The value "*" indicates that the import request is for the target module’s namespace object.
        AtomicString m_importName;
        // [[LocalName]]   String  The name that is used to locally access the imported value from within the importing module.
        AtomicString m_localName;
    };
    typedef Vector<ImportEntry, GCUtil::gc_malloc_allocator<ImportEntry>> ImportEntryVector;

    // https://tc39.es/ecma262/#exportentry-record
    struct ExportEntry {
        // [[ExportName]]  String  The name used to export this binding by this module.
        Optional<AtomicString> m_exportName;
        // [[ModuleRequest]]   ModuleRequest | null   The value of the ModuleSpecifier of the ExportDeclaration. null if the ExportDeclaration does not have a ModuleSpecifier.
        Optional<ModuleRequest> m_moduleRequest;
        // [[ImportName]]  String | null   The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. null if the ExportDeclaration does not have a ModuleSpecifier. "*" indicates that the export request is for all exported bindings.
        Optional<AtomicString> m_importName;
        // [[LocalName]]   String | null   The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
        Optional<AtomicString> m_localName; // The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
    };
    typedef Vector<ExportEntry, GCUtil::gc_malloc_allocator<ExportEntry>> ExportEntryVector;

    struct ModuleData : public gc {
        bool m_didCallLoadedCallback;
        // Abstract Module Records
        // https://tc39.es/ecma262/#sec-abstract-module-records
        // [[Environment]]
        ModuleEnvironmentRecord* m_moduleRecord; // this record contains namespace object
        // [[Namespace]]
        Optional<ModuleNamespaceObject*> m_namespace;

        // Source Text Module Records
        // https://tc39.es/ecma262/#sec-source-text-module-records
        // [[ImportEntries]]
        ImportEntryVector m_importEntries;
        // [[LocalExportEntries]]
        ExportEntryVector m_localExportEntries;
        // [[IndirectExportEntries]]
        ExportEntryVector m_indirectExportEntries;
        // [[StarExportEntries]]
        ExportEntryVector m_starExportEntries;
        // [[ImportMeta]]
        Optional<Object*> m_importMeta;

        // Cyclic Module Records
        // https://tc39.es/ecma262/#table-cyclic-module-fields
        enum ModuleStatus {
            Unlinked,
            Linking,
            Linked,
            Evaluating,
            Evaluated
        };
        // [[Status]]
        ModuleStatus m_status;
        // [[EvaluationError]]
        Optional<EncodedValue> m_evaluationError;
        // [[DFSIndex]]
        Optional<uint32_t> m_dfsIndex;
        // [[DFSAncestorIndex]]
        Optional<uint32_t> m_dfsAncestorIndex;
        // [[RequestedModules]] is same with moduleRequests
        ModuleRequestVector m_requestedModules;

        // + https://tc39.es/proposal-top-level-await/#sec-cyclic-module-records
        // [[CycleRoot]]
        Optional<Script*> m_cycleRoot;
        // [[Async]] -> has same value with m_topCodeBlock->isAsync()
        // [[AsyncEvaluating]]
        bool m_asyncEvaluating;
        // [[TopLevelCapability]]
        Optional<PromiseReaction::Capability> m_topLevelCapability;
        // [[AsyncParentModules]]
        Vector<Script*, GCUtil::gc_malloc_allocator<Script*>> m_asyncParentModules;
        // [[PendingAsyncDependencies]]
        Optional<size_t> m_pendingAsyncDependencies;

        Vector<PromiseObject*, GCUtil::gc_malloc_allocator<PromiseObject*>> m_asyncPendingPromises;

        class ModulePromiseObject : public PromiseObject {
        public:
            explicit ModulePromiseObject(ExecutionState& state, Object* proto)
                : PromiseObject(state, proto)
                , m_referrer(nullptr)
                , m_loadedScript(nullptr)
            {
            }
            void* operator new(size_t size);
            void* operator new[](size_t size) = delete;
            Script* m_referrer;
            Script* m_loadedScript;
            EncodedValue m_value;
        };

        ModuleData()
            : m_didCallLoadedCallback(false)
            , m_moduleRecord(nullptr)
            , m_status(Unlinked)
            , m_asyncEvaluating(false)
        {
        }
    };

    Script(String* srcName, String* sourceCode, ModuleData* moduleData, size_t originLineOffset, bool canExecuteAgain
#if defined(ENABLE_CODE_CACHE)
           ,
           size_t sourceCodeHashValue = 0
#endif
           )
        : m_canExecuteAgain(canExecuteAgain && !moduleData)
#if defined(ENABLE_CODE_CACHE)
        , m_sourceCodeHashValue(sourceCodeHashValue)
#endif
        , m_srcName(srcName)
        , m_sourceCode(sourceCode)
        , m_topCodeBlock(nullptr)
        , m_moduleData(moduleData)
        , m_originSourceLineOffset(originLineOffset)
    {
        // srcName and sourceCode should have valid string (empty string for no name)
        ASSERT(!!srcName && !!sourceCode);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

    Value execute(ExecutionState& state, bool isExecuteOnEvalFunction = false, bool inStrictMode = false);
    String* srcName()
    {
        return m_srcName;
    }

    String* sourceCode()
    {
        return m_sourceCode;
    }

    InterpretedCodeBlock* topCodeBlock()
    {
        return m_topCodeBlock;
    }

    Context* context();

    bool isModule()
    {
        return m_moduleData;
    }

    ModuleData* moduleData()
    {
        return m_moduleData;
    }

#if defined(ENABLE_CODE_CACHE)
    size_t sourceCodeHashValue()
    {
        if (UNLIKELY(m_sourceCodeHashValue == 0)) {
            m_sourceCodeHashValue = m_sourceCode->hashValue();
        }
        return m_sourceCodeHashValue;
    }
#endif

    size_t moduleRequestsLength();
    String* moduleRequest(size_t i);

    Value moduleInstantiate(ExecutionState& state);
    Value moduleEvaluate(ExecutionState& state);

    bool isExecuted();
    bool wasThereErrorOnModuleEvaluation();
    Value moduleEvaluationError();

    // https://tc39.es/ecma262/#sec-meta-properties-runtime-semantics-evaluation
    Object* importMetaProperty(ExecutionState& state);

    // https://tc39.es/ecma262/#sec-getmodulenamespace
    ModuleNamespaceObject* getModuleNamespace(ExecutionState& state);

    size_t originSourceLineOffset() const { return m_originSourceLineOffset; }

private:
    Value executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isStrictModeOutside = false, bool isEvalCodeOnFunction = false);
    Script* loadModuleFromScript(ExecutionState& state, ModuleRequest& request);
    void loadExternalModule(ExecutionState& state);
    Value executeModule(ExecutionState& state, Optional<Script*> referrer);
    struct ResolveExportResult {
        enum ResultType {
            Record,
            Null,
            Ambiguous
        };
        ResultType m_type;
        Optional<std::tuple<Script*, AtomicString>> m_record;

        ResolveExportResult(ResultType tp, Optional<std::tuple<Script*, AtomicString>> record = Optional<std::tuple<Script*, AtomicString>>())
            : m_type(tp)
            , m_record(record)
        {
        }
    };

    ResolveExportResult resolveExport(ExecutionState& state, AtomicString exportName)
    {
        std::vector<std::tuple<Script*, AtomicString>> resolveSet;
        return resolveExport(state, exportName, resolveSet);
    }

    // https://tc39.es/ecma262/#sec-resolveexport
    ResolveExportResult resolveExport(ExecutionState& state, AtomicString exportName, std::vector<std::tuple<Script*, AtomicString>>& resolveSet);
    AtomicStringVector exportedNames(ExecutionState& state)
    {
        std::vector<Script*> exportStarSet;
        return exportedNames(state, exportStarSet);
    }
    // https://tc39.es/ecma262/#sec-getexportednames
    AtomicStringVector exportedNames(ExecutionState& state, std::vector<Script*>& exportStarSet);

    struct ModuleExecutionResult {
        bool gotException;
        Value value;

        ModuleExecutionResult(bool gotException, Value value)
            : gotException(gotException)
            , value(value)
        {
        }
    };
    // https://tc39.es/ecma262/#sec-moduledeclarationlinking
    ModuleExecutionResult moduleLinking(ExecutionState& state);

    // https://tc39.es/ecma262/#sec-InnerModuleLinking
    ModuleExecutionResult innerModuleLinking(ExecutionState& state, std::vector<Script*>& stack, uint32_t index);

    // https://tc39.es/ecma262/#sec-moduleevaluation
    ModuleExecutionResult moduleEvaluation(ExecutionState& state);

    // https://tc39.es/ecma262/#sec-innermoduleevaluation
    ModuleExecutionResult innerModuleEvaluation(ExecutionState& state, std::vector<Script*>& stack, uint32_t index);

    // https://tc39.es/ecma262/#sec-source-text-module-record-initialize-environment
    Value moduleInitializeEnvironment(ExecutionState& state);

    // https://tc39.es/ecma262/#sec-source-text-module-record-execute-module
    // returns gotException and Value
    ModuleExecutionResult moduleExecute(ExecutionState& state, Optional<PromiseReaction::Capability> capability = nullptr);

    // https://tc39.es/proposal-top-level-await/#sec-execute-async-module
    void moduleExecuteAsyncModule(ExecutionState& state);

    static void asyncModuleFulfilled(ExecutionState& state, Script* module);
    static void asyncModuleRejected(ExecutionState& state, Script* module, Value error);
    static Value asyncModuleFulfilledFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
    static Value asyncModuleRejectedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

    bool m_canExecuteAgain;
#if defined(ENABLE_CODE_CACHE)
    size_t m_sourceCodeHashValue;
#endif
    String* m_srcName;
    String* m_sourceCode;
    InterpretedCodeBlock* m_topCodeBlock;
    ModuleData* m_moduleData;

    // original source code's start line offset
    // default value is zero, but it has other value for source codes manipulated by `createFunctionScript`
    size_t m_originSourceLineOffset;
};

} // namespace Escargot

#endif
