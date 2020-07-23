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

#include "runtime/Value.h"
#if defined(ENABLE_CODE_CACHE)
#include "codecache/CodeCache.h"
#endif

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
    // http://www.ecma-international.org/ecma-262/6.0/#table-39
    struct ImportEntry {
        // [[ModuleRequest]]   String  String value of the ModuleSpecifier of the ImportDeclaration.
        String* m_moduleRequest;
        // [[ImportName]]  String  The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. The value "*" indicates that the import request is for the target module’s namespace object.
        AtomicString m_importName;
        // [[LocalName]]   String  The name that is used to locally access the imported value from within the importing module.
        AtomicString m_localName;

        ImportEntry()
            : m_moduleRequest(String::emptyString)
        {
        }
    };
    typedef Vector<ImportEntry, GCUtil::gc_malloc_allocator<ImportEntry>> ImportEntryVector;

    // http://www.ecma-international.org/ecma-262/6.0/#table-41
    struct ExportEntry {
        // [[ExportName]]  String  The name used to export this binding by this module.
        Optional<AtomicString> m_exportName;
        // [[ModuleRequest]]   String | null   The String value of the ModuleSpecifier of the ExportDeclaration. null if the ExportDeclaration does not have a ModuleSpecifier.
        Optional<String*> m_moduleRequest;
        // [[ImportName]]  String | null   The name under which the desired binding is exported by the module identified by [[ModuleRequest]]. null if the ExportDeclaration does not have a ModuleSpecifier. "*" indicates that the export request is for all exported bindings.
        Optional<AtomicString> m_importName;
        // [[LocalName]]   String | null   The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
        Optional<AtomicString> m_localName; // The name that is used to locally access the exported value from within the importing module. null if the exported value is not locally accessible from within the module.
    };
    typedef Vector<ExportEntry, GCUtil::gc_malloc_allocator<ExportEntry>> ExportEntryVector;

    struct ModuleData : public gc {
        bool m_didCallLoadedCallback;
        // Abstract Module Records
        // https://www.ecma-international.org/ecma-262/#sec-abstract-module-records
        // [[Environment]]
        ModuleEnvironmentRecord* m_moduleRecord; // this record contains namespace object
        // [[Namespace]]
        Optional<ModuleNamespaceObject*> m_namespace;

        // Source Text Module Records
        // https://www.ecma-international.org/ecma-262/#sec-source-text-module-records
        // [[ImportEntries]]
        ImportEntryVector m_importEntries;
        // [[LocalExportEntries]]
        ExportEntryVector m_localExportEntries;
        // [[IndirectExportEntries]]
        ExportEntryVector m_indirectExportEntries;
        // [[StarExportEntries]]
        ExportEntryVector m_starExportEntries;

        // Cyclic Module Records
        // https://www.ecma-international.org/ecma-262/#sec-cyclic-module-records
        enum ModuleStatus {
            Uninstantiated,
            Instantiating,
            Instantiated,
            Evaluating,
            Evaluated
        };
        // [[Status]]
        ModuleStatus m_status;
        // [[EvaluationError]]
        bool m_hasEvaluationError;
        EncodedValue m_evaluationError;
        // [[DFSIndex]]
        Optional<uint32_t> m_dfsIndex;
        // [[DFSAncestorIndex]]
        Optional<uint32_t> m_dfsAncestorIndex;
        // [[RequestedModules]] is same with moduleRequests
        StringVector m_requestedModules;
        // [[ImportMeta]]
        Optional<Object*> m_importMeta;

        ModuleData()
            : m_didCallLoadedCallback(false)
            , m_moduleRecord(nullptr)
            , m_status(Uninstantiated)
            , m_hasEvaluationError(false)
        {
        }
    };

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

    size_t moduleRequestsLength();
    String* moduleRequest(size_t i);

    bool isExecuted();
    bool wasThereErrorOnModuleEvaluation();
    Value moduleEvaluationError();

    // https://www.ecma-international.org/ecma-262/#sec-meta-properties-runtime-semantics-evaluation
    Object* importMetaProperty(ExecutionState& state);

    // https://www.ecma-international.org/ecma-262/#sec-getmodulenamespace
    ModuleNamespaceObject* getModuleNamespace(ExecutionState& state);

#if defined(ENABLE_CODE_CACHE)
    void setCodeCacheMetaInfo(CodeCacheMetaInfoMap* codeBlockMap, CodeCacheMetaInfoMap* byteCodeMap)
    {
        ASSERT(!m_codeBlockMetaInfoMap && !m_byteCodeMetaInfoMap);
        m_codeBlockMetaInfoMap = codeBlockMap;
        m_byteCodeMetaInfoMap = byteCodeMap;
    }

    CodeCacheMetaInfoMap* codeBlockMetaInfoMap()
    {
        return m_codeBlockMetaInfoMap;
    }

    CodeCacheMetaInfoMap* byteCodeMetaInfoMap()
    {
        return m_byteCodeMetaInfoMap;
    }
#endif

private:
    Script(String* srcName, String* sourceCode, ModuleData* moduleData, bool canExecuteAgain)
        : m_canExecuteAgain(canExecuteAgain && !moduleData)
        , m_srcName(srcName)
        , m_sourceCode(sourceCode)
        , m_topCodeBlock(nullptr)
        , m_moduleData(moduleData)
#if defined(ENABLE_CODE_CACHE)
        , m_codeBlockMetaInfoMap(nullptr)
        , m_byteCodeMetaInfoMap(nullptr)
#endif
    {
    }

    Value executeLocal(ExecutionState& state, Value thisValue, InterpretedCodeBlock* parentCodeBlock, bool isStrictModeOutside = false, bool isEvalCodeOnFunction = false);
    Script* loadModuleFromScript(ExecutionState& state, String* src);
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

    // https://www.ecma-international.org/ecma-262/#sec-resolveexport
    ResolveExportResult resolveExport(ExecutionState& state, AtomicString exportName, std::vector<std::tuple<Script*, AtomicString>>& resolveSet);
    AtomicStringVector exportedNames(ExecutionState& state)
    {
        std::vector<Script*> exportStarSet;
        return exportedNames(state, exportStarSet);
    }
    // http://www.ecma-international.org/ecma-262/6.0/#sec-getexportednames
    AtomicStringVector exportedNames(ExecutionState& state, std::vector<Script*>& exportStarSet);

    struct ModuleExecutionResult {
        bool gotExecption;
        Value value;

        ModuleExecutionResult(bool gotExecption, Value value)
            : gotExecption(gotExecption)
            , value(value)
        {
        }
    };
    // https://www.ecma-international.org/ecma-262/#sec-moduledeclarationinstantiation
    ModuleExecutionResult moduleInstantiate(ExecutionState& state);

    // https://www.ecma-international.org/ecma-262/#sec-innermoduleinstantiation
    ModuleExecutionResult innerModuleInstantiation(ExecutionState& state, std::vector<Script*>& stack, uint32_t index);

    // https://www.ecma-international.org/ecma-262/#sec-moduleevaluation
    ModuleExecutionResult moduleEvaluate(ExecutionState& state);

    // https://www.ecma-international.org/ecma-262/#sec-innermoduleevaluation
    ModuleExecutionResult innerModuleEvaluation(ExecutionState& state, std::vector<Script*>& stack, uint32_t index);

    // https://www.ecma-international.org/ecma-262/#sec-source-text-module-record-initialize-environment
    Value moduleInitializeEnvironment(ExecutionState& state);

    // https://www.ecma-international.org/ecma-262/#sec-source-text-module-record-execute-module
    // returns gotExecption and Value
    ModuleExecutionResult moduleExecute(ExecutionState& state);

    bool m_canExecuteAgain;
    String* m_srcName;
    String* m_sourceCode;
    InterpretedCodeBlock* m_topCodeBlock;
    ModuleData* m_moduleData;
#if defined(ENABLE_CODE_CACHE)
    CodeCacheMetaInfoMap* m_codeBlockMetaInfoMap;
    CodeCacheMetaInfoMap* m_byteCodeMetaInfoMap;
#endif
};
}

#endif
